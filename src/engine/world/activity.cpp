#include "engine/world/activity.hpp"

#include "engine/audio/audio.hpp"
#include "engine/usecode/world_intrinsics.hpp"
#include "engine/world/actor_anim.hpp"
#include "engine/world/attack.hpp"
#include "engine/world/collision.hpp"
#include "engine/world/pathfind.hpp"

#include <algorithm>
#include <cstdlib>
#include <memory>

namespace crusader {

namespace {

// random_range(n) (SYSTEM.C 12D8:04FF), UCMachine::I_rndRange (12E0:0293)
// and the C library's rand() * n / 0x8000 (core/random.hpp), each where the
// original uses it.
int rnd(World& w, int n) { return random_range(w.state.random_state, static_cast<std::uint16_t>(n)); }
int crt_rnd(World& w, int n) { return crt_rand_below(w.state.crt_rand_state, n); }

int rnd_between(World& w, int lo, int hi) {
    return random_between(w.state.random_state, static_cast<std::int16_t>(lo), static_cast<std::int16_t>(hi));
}

std::uint16_t word_at(std::span<const std::uint8_t> r, std::size_t at) {
    return static_cast<std::uint16_t>(r[at] | r[at + 1] << 8);
}

// TYPE_1108_026e: in the combat zone (the screen), with a default activity 1 it is not doing yet,
// the NPC that sees the controlled NPC takes its default activity 1.
// @orig 1118:033E
bool spot_controlled(Kernel& kernel, World& world, const AnimDat& anims, ObjId npc) {
    const Item* self = world.get(npc);
    const auto r = world.npc_record(npc);
    if (!self || r.empty() || !world.is_partly_in_combat_zone(*self)) return false;
    const std::uint16_t alert = word_at(r, npc::kDefaultActivity + 2);
    if (alert == 0 || word_at(r, npc::kActivity) == alert) return false;
    if (!sees_controlled(world, anims, npc)) return false;
    set_activity(kernel, world, anims, npc, 0x71);
    return true;
}

// TYPE_1108_0000: the common start of activity processes. They are timer
// processes (TimerProcess, 11E8:1814); only the loiter one starts its timer.
// No Regret (1118:0000) first sets when the NPC's next combat remark is due
// (RemarkTimer), and no longer puts the weapon away (record +0x78 bit 1).
// @orig 1118:0000
class ActivityProcess : public TimedProcess {
public:
    ActivityProcess(World& world, const AnimDat& anims, ObjId npc, std::uint16_t type) : world_(world), anims_(anims) {
        set_owner(npc);
        set_type(type);
    }

    bool runs_in_slice() const override { return true; }

protected:
    // TYPE_1108_0d3e, event 2 (a timer period): a placed, living NPC that
    // sees the controlled NPC takes its default activity 1 (spot_controlled).
    // Event 1 (idle actions 0x33 / 0x34) is sent by nothing in this version.
    // @orig 1118:0CEC
    void tick() override {
        const Item* self = world_.get(owner());
        if (!self || (self->z & 0xFF) == 0xFE || world_.is_dead(owner()) || kernel().failing_all()) return;
        spot_controlled(kernel(), world_, anims_, owner());
    }

    void on_added() override {
        if (!world_.remorse()) remarks_.init(world_, kernel().ticks());
        const Item* self = world_.get(owner());
        if (!self || !anims_.action(self->shape, 0)) return;
        set_pose(world_, anims_, owner(), 0);
        if (!world_.remorse()) return;
        if (auto r = world_.npc_record(owner()); !r.empty()) r[npc::kStance] &= static_cast<std::uint8_t>(~0x02);
    }

    World& world_;
    const AnimDat& anims_;
    RemarkTimer remarks_;
};

// Activity 1 (1108:0359).
// @orig 1118:0429
class StandProcess : public ActivityProcess {
public:
    StandProcess(World& w, const AnimDat& a, ObjId npc) : ActivityProcess(w, a, npc, 0x258) {}

protected:
    void run() override {
        if (!spot_controlled(kernel(), world_, anims_, owner())) sleep(60);
    }
};

// Activity 2 (1108:0BFA): `count` walks (0xFFFF: no end).
// @orig 1118:0BA8
class LoiterProcess : public ActivityProcess {
public:
    LoiterProcess(World& w, const AnimDat& a, ObjId npc, std::uint16_t count)
        : ActivityProcess(w, a, npc, 0x257), count_(count) {
        if (const Item* self = w.get(npc)) home_ = w.location(*self);
        // 1108:0AFB: a 1 Hz timer, also while it waits for its walks.
        set_rate(1);
    }

protected:
    void run() override {
        const ObjId npc = owner();
        if (actor_busy(kernel(), npc)) return;
        if (count_ != 0xFFFF && count_-- == 0) {
            terminate(0);
            return;
        }
        const Item* self = world_.get(npc);
        const bool can_fidget = self && anims_.action(self->shape, 0x1E);
        if (rnd(world_, 2) == 0 && can_fidget) {
            fidget(kernel(), world_, anims_, npc);
            return;
        }
        const int dx = rnd_between(world_, -0xA0, 0xA0);
        const int dy = rnd_between(world_, -0xA0, 0xA0);
        const Pid walk = walk_to(kernel(), world_, anims_, npc, 0, {home_.x + dx, home_.y + dy, home_.z}, 0xC, 0x40, false);
        if (walk != 0) wait_for(walk);
    }

private:
    std::uint16_t count_;
    World::Point home_{};
};

// Activity 3 (1108:0708).
// @orig 1118:06B6 1118:0641
class WalkProcess : public ActivityProcess {
public:
    WalkProcess(World& w, const AnimDat& a, ObjId npc) : ActivityProcess(w, a, npc, 0x255) {}

protected:
    void run() override {
        Kernel& k = kernel();
        const ObjId npc = owner();
        const Item* self = world_.get(npc);
        if (!self || spot_controlled(k, world_, anims_, npc) || actor_busy(k, npc)) return;
        const int dir = world_.npc_direction(npc) & 0xF;
        World::Point pos = world_.location(*self);
        if (test_action(world_, anims_, npc, 1, dir, pos).ok) {
            blocked_ = 0;
            const Pid pid = do_anim(k, world_, anims_, npc, 1, kDirCurrent, 0, false);
            if (pid != 0) wait_for(pid);
            return;
        }
        if (++blocked_ > 1) {
            // Blocked twice: shapes 0x2F5, 0x2F6, 0x2F7, 0x344 and 0x597 take
            // activity 7, the others attack (5).
            const std::uint16_t s = self->shape;
            const bool listed = s == 0x2F5 || s == 0x2F7 || s == 0x2F6 || s == 0x344 || s == 0x597;
            set_activity(k, world_, anims_, npc, listed ? 7 : 5);
            return;
        }
        do_anim(k, world_, anims_, npc, 0, kDirCurrent, 0, false);
        const Pid turn = turn_to(k, world_, anims_, npc, (dir + 8) & 0xF, false, false);
        if (Process* p = k.get(turn)) {
            p->sleep(60);
            wait_for(turn);
        }
    }

private:
    int blocked_ = 0;  // +0x44
};

// Activity 7 (1108:0383 / 04AB / 0437). No Regret (1118:0576) says a
// combat remark (combat_remark) each time one is due (RemarkTimer) instead of
// one plea from the tables below, still only when it sees the controlled NPC.
// @orig 1118:0453 1118:0576 1118:0502
class SurrenderProcess : public ActivityProcess {
public:
    SurrenderProcess(World& w, const AnimDat& a, ObjId npc) : ActivityProcess(w, a, npc, 0x25F) {}

protected:
    void on_added() override {
        ActivityProcess::on_added();
        do_anim(kernel(), world_, anims_, owner(), 0x39, kDirCurrent, 0, false);
        if (auto r = world_.npc_record(owner()); !r.empty()) r[npc::kStance] |= 0x01;
    }

    void run() override {
        Kernel& k = kernel();
        const ObjId npc = owner();
        const ObjId controlled = world_.state.controlled_npc;
        const Item* self = world_.get(npc);
        const Item* other = world_.get(controlled);
        if (self && other && !actor_busy(k, npc) && world_.is_mostly_in_combat_zone(*self)) {
            const int dir = direction_8(self->x, self->y, other->x, other->y);
            if ((world_.npc_direction(npc) & 0xF) == dir) {
                // Once it sees the controlled NPC, a plea drawn by shape.
                if (!world_.remorse()) {
                    if (remarks_.due(k.ticks()) && range_if_visible(world_, npc, controlled) != 0)
                        combat_remark(world_, npc);
                } else if (!barked_ && range_if_visible(world_, npc, controlled) != 0) {
                    barked_ = true;
                    plead(*self);
                }
            } else {
                turn_to(k, world_, anims_, npc, dir, true, false);  // Actor::I_turnToward
            }
        }
        sleep(60);
    }

    void on_terminate() override {
        if (auto r = world_.npc_record(owner()); !r.empty()) r[npc::kStance] &= static_cast<std::uint8_t>(~0x01);
    }

private:
    // Tables DS:1EA4, 1EB0, 1EB8, 1EC0, 1EC8; other shapes say nothing.
    void plead(const Item& self) {
        static constexpr int k2F7[] = {0xE9, 0xE0, 0xEB, 0xE2, 0xE1, 0xEA};
        static constexpr int k2F6[] = {0xB4, 0xC5, 0xC6, 0xE8};
        static constexpr int k344[] = {0xE3, 0xE4, 0xEC, 0xF6};
        static constexpr int k2F5[] = {0xDE, 0xDF, 0x8A, 0x8B};
        static constexpr int k597[] = {0xD6, 0xFF, 0xD7};
        int sfx = -1;
        switch (self.shape) {
        case 0x2F7: sfx = k2F7[crt_rnd(world_, 6)]; break;
        case 0x2F6: sfx = k2F6[crt_rnd(world_, 4)]; break;
        case 0x344: sfx = k344[crt_rnd(world_, 4)]; break;
        case 0x2F5: sfx = k2F5[crt_rnd(world_, 4)]; break;
        case 0x597: sfx = k597[crt_rnd(world_, 3)]; break;
        default: return;
        }
        if (Audio* audio = world_.audio()) audio->sources.play(world_, sfx, owner());
    }

    bool barked_ = false;  // +0x42
};

// Activity 8 (1108:0A0E).
// @orig 1118:09BC
class GuardProcess : public ActivityProcess {
public:
    GuardProcess(World& w, const AnimDat& a, ObjId npc) : ActivityProcess(w, a, npc, 0x25E) {}

protected:
    void run() override {
        Kernel& k = kernel();
        const ObjId npc = owner();
        if (actor_busy(k, npc)) {
            sleep(60);
            return;
        }
        if (sees_controlled(world_, anims_, npc)) {
            set_activity(k, world_, anims_, npc, 5);
            return;
        }
        if (rnd(world_, 2) == 0) {
            sleep(static_cast<std::uint32_t>(crt_rnd(world_, 3) * 60 + 60));
            return;
        }
        const Item* self = world_.get(npc);
        if (self && anims_.action(self->shape, 0x1E)) fidget(k, world_, anims_, npc);
    }
};

// No Regret, activity 0xD ("RollingThunder", type 0x263, 1118:0EF5): an NPC
// that rolls aside, fires one to three shots and rolls back. Each second it
// is not busy:
// - its target becomes the controlled NPC (the player when there is none)
//   unless it is that NPC or a decoy (shape 0x584) still on the map; a dead
//   or vanished target is only replaced, the NPC then waits;
// - mostly on screen, it tries a roll to one side drawn by rand(), then the
//   other (roll): the roll must be possible, end mostly on screen, facing
//   the target within two steps of its direction, and from there the
//   controlled NPC must be in its line of fire (fire_distance_from) in that
//   direction or one or two steps aside. It then rolls, turns to that
//   direction, fires rand(1..3) times, each shot ending at once unless the
//   NPC faces the controlled NPC (ActorAnimProcess::face_target), and rolls
//   back, and waits for that;
// - without a roll, facing the target it fires when hit within the last 120
//   ticks or in the line of fire, else may take a decoy as target
//   (aim_at_decoy); not facing it, it turns to it.
// set_activity does not end it (type 0x263 is not in its list).
// @orig 1118:0EF5 1118:0FD9
class RollingThunderProcess : public ActivityProcess {
public:
    static constexpr std::uint16_t kType = 0x263;
    RollingThunderProcess(World& w, const AnimDat& a, ObjId npc) : ActivityProcess(w, a, npc, kType) {}

protected:
    void run() override {
        Kernel& k = kernel();
        const ObjId npc = owner();
        if (actor_busy(k, npc)) {
            sleep(60);
            return;
        }
        const ObjId controlled = world_.state.controlled_npc;
        auto gone = [this](ObjId id) {
            const Item* i = world_.get(id);
            return id == 0 || !i || (i->z & 0xFF) == 0xFE;
        };
        auto shape_of = [this](ObjId id) {
            const Item* i = world_.get(id);
            return i ? i->shape : std::uint16_t{0};
        };
        ObjId target = world_.npc_word(npc, npc::kTarget);
        if (target != controlled && (gone(target) || shape_of(target) != kDecoy))
            set_target(k, world_, anims_, npc, controlled == 0 ? 1 : controlled);
        target = world_.npc_word(npc, npc::kTarget);
        if ((shape_of(target) != kDecoy && world_.is_dead(target)) || gone(target)) {
            // The test after the new target reads the old one: always true.
            if (target != controlled) set_target(k, world_, anims_, npc, controlled);
            sleep(60);
            return;
        }
        const Item* self = world_.get(npc);
        if (self && world_.is_mostly_on_screen(*self)) {
            const bool left_first = crt_rnd(world_, 2) != 0;
            const int facing = world_.npc_direction(npc) & 0xF;
            std::uint16_t action = left_first ? 0x3D : 0x3E;
            int roll_dir = 0;
            bool rolled = roll(action, roll_dir);
            if (!rolled) {
                action = left_first ? 0x3E : 0x3D;
                rolled = roll(action, roll_dir);
            }
            if (rolled) {
                do_anim(k, world_, anims_, npc, action, kDirCurrent, 0, false);
                if (roll_dir != facing) turn_to(k, world_, anims_, npc, roll_dir, true, false);
                const int shots = crt_rnd(world_, 3) + 1;
                for (int i = 0; i < shots; ++i) {
                    const Pid shot = do_anim(k, world_, anims_, npc, 8, kDirCurrent, 10000, false);
                    if (auto* p = dynamic_cast<ActorAnimProcess*>(k.get(shot))) p->face_target(controlled);
                }
                const Pid back = do_anim(k, world_, anims_, npc, action == 0x3D ? 0x3E : 0x3D, kDirCurrent, 10000, false);
                if (back != 0) wait_for(back);
            } else if (const Item* t = world_.get(target); t && (self = world_.get(npc))) {
                const int toward = closest_direction_in_range(self->x, self->y, t->x, t->y, 16, 0, 15);
                if (toward == (world_.npc_direction(npc) & 0xF)) {
                    if (recently_hit(world_, npc, k.ticks()) || fire_distance(world_, anims_, npc, target, toward) != 0) {
                        do_anim(k, world_, anims_, npc, 8, kDirCurrent, 0, false);
                        return;
                    }
                    if (aim_at_decoy()) return;
                } else {
                    const auto r = world_.npc_record(npc);
                    const bool relaxed = r.empty() || !(r[npc::kStance] & 0x02);
                    const Pid turn = turn_to(k, world_, anims_, npc, toward, true, relaxed);
                    if (turn != 0) {
                        wait_for(turn);
                        return;
                    }
                }
            }
        }
        sleep(60);
    }

private:
    static constexpr std::uint16_t kDecoy = 0x584;

    // 1118:16BE: can the NPC roll with `action` (0x3D, 0x3E) and fire at the
    // controlled NPC from where it ends? `dir` is then the direction from
    // there to its target.
    bool roll(std::uint16_t action, int& dir) {
        const ObjId npc = owner();
        const Item* self = world_.get(npc);
        const Item* target = world_.get(world_.npc_word(npc, npc::kTarget));
        if (!self || !world_.is_mostly_on_screen(*self)) return false;
        const int facing = world_.npc_direction(npc) & 0xF;
        const ActionTest test = test_action(world_, anims_, npc, action, facing, world_.location(*self));
        if (!test.ok) return false;
        // ITEM_10b0_40b4: the NPC would be mostly on screen there.
        Item there = *self;
        there.x = test.end.x;
        there.y = test.end.y;
        there.z = test.end.z;
        if (!world_.is_mostly_on_screen(there)) return false;
        dir = target ? closest_direction_in_range(test.end.x, test.end.y, target->x, target->y, 16, 0, 15) : 0;
        const int d = dir - facing;  // without wrapping, as the original
        if (d != 0 && d != 1 && d != -1 && d != 2 && d != -2) return false;
        const ObjId controlled = world_.state.controlled_npc;
        for (int step : {0, -1, 1, -2, 2}) {
            if (fire_distance_from(world_, anims_, npc, controlled, (dir + step) & 0xF, test.end) != 0) return true;
        }
        return false;
    }

    // 1118:154D: at most every 90 ticks (1118:0F8B), a decoy in the line of
    // fire becomes the target.
    bool aim_at_decoy() {
        Kernel& k = kernel();
        const ObjId npc = owner();
        const Item* target = world_.get(world_.npc_word(npc, npc::kTarget));
        if (target && target->shape == kDecoy) return false;
        const std::uint32_t now = k.ticks();
        if (now < decoy_time_ + 0x5A) return false;
        decoy_time_ = now;
        const ObjId decoy = find_decoy(world_, npc);
        const Item* self = world_.get(npc);
        const Item* d = world_.get(decoy);
        if (!self || !d) return false;
        const int dir = closest_direction_in_range(self->x, self->y, d->x, d->y, 16, 0, 15);
        if (fire_distance(world_, anims_, npc, decoy, dir) == 0) return false;
        set_target(k, world_, anims_, npc, decoy);
        return true;
    }

    std::uint32_t decoy_time_ = 0;  // +0x48
};

}  // namespace

// No Regret, activity 0xD (set_activity).
Pid start_rolling_thunder(Kernel& kernel, World& world, const AnimDat& anims, ObjId npc) {
    return kernel.add(std::make_unique<RollingThunderProcess>(world, anims, npc));
}

// @orig 1118:0192
bool sees_controlled(const World& world, const AnimDat& anims, ObjId npc, bool look_turn) {
    const ObjId controlled = world.state.controlled_npc;
    const Item* self = world.get(npc);
    const Item* other = world.get(controlled);
    if (controlled == 0 || !self || !other || !world.is_mostly_in_combat_zone(*self)) return false;
    int facing = world.npc_direction(npc) & 0xF;
    if (look_turn) {
        // Looking left or right (actions 0x1E, 0x1F) from its third frame,
        // the NPC sees four directions aside.
        const auto r = world.npc_record(npc);
        const int last = r[npc::kLastAnim] | r[npc::kLastAnim + 1] << 8;
        if ((last == 0x1E || last == 0x1F) && r[npc::kAnimFrame] >= 2) {
            const bool south = facing == 8 || facing == 10 || facing == 12;
            const bool back = last == 0x1E ? !south : south;
            facing = (facing + (back ? -4 : 4)) & 0xF;
        }
    }
    const int d = direction_8(self->x, self->y, other->x, other->y) & 0xF;
    bool near = false;
    for (int k = -2; k <= 2; ++k) near = near || facing == ((d + k) & 0xF);
    if (!near) return false;
    // The line of fire is looked for towards the controlled NPC, whatever
    // the facing within the two directions.
    bool muzzle_found = false;
    if (fire_distance(world, anims, npc, controlled, d, 0, 0, 0, &muzzle_found) != 0) return true;
    return !muzzle_found && range_if_visible(world, npc, controlled) != 0;
}

// @orig 1140:258E
int range_if_visible(const World& world, ObjId item_id, ObjId other_id) {
    const Item* item = world.get(item_id);
    const Item* other = world.get(other_id);
    if (!item || !other) return 0;
    const World::Point from = world.location(*item);
    World::Point to = world.location(*other);
    const ShapeInfo* info = world.shape_info(other->shape);
    const int height = info ? info->footpad_z : 0;
    if (height > 1) to.z = (to.z + (height - 1) * 8) & 0xFF;
    const SweepResult r = sweep(world, item->shape, item_id, from, to, kSweepNoGround);
    if (r.blocker != other_id && r.blocker != 0) return 0;
    int distance = std::max({std::abs(static_cast<std::int16_t>(from.x - to.x)),
                             std::abs(static_cast<std::int16_t>(from.y - to.y)), std::abs(from.z - to.z)}) / 16;
    if (distance & 0xF) ++distance;
    return distance < 0x31 ? distance : 0;
}

// The shapes that must see the controlled NPC to wake: 0x2F5..0x2F7 in both
// games, then 0x384 and 0x344 in No Regret (0x595 and 0x597 in No Remorse).
// @orig 1148:3BD3
void wake_npcs_on_screen(Kernel& kernel, World& world, const AnimDat& anims) {
    const ObjId controlled = world.state.controlled_npc;
    for (ObjId id = 2; id < 0x100; ++id) {
        const Item* self = world.get(id);
        if (!self || (self->z & 0xFF) == 0xFE || id == controlled || !world.is_partly_in_combat_zone(*self))
            continue;
        const auto r = world.npc_record(id);
        if (r.empty()) continue;
        const std::uint16_t activity = word_at(r, npc::kActivity);
        const bool settled = in_combat(world, id) || activity == word_at(r, npc::kDefaultActivity + 4) ||
                             (r[npc::kStance] & 0x01);
        if (settled) {
            const bool guard = activity == 8;
            if (guard) set_activity(kernel, world, anims, id, 5);
            if (guard || in_combat(world, id)) set_target(kernel, world, anims, id, controlled);
            continue;
        }
        const std::uint16_t s = self->shape;
        const bool looks = s == 0x2F5 || s == 0x2F6 || s == 0x2F7 ||
                           (world.remorse() ? s == 0x595 || s == 0x597 : s == 0x384 || s == 0x344);
        if (looks && range_if_visible(world, id, controlled) == 0) continue;
        set_activity(kernel, world, anims, id, 0x72);
        if (in_combat(world, id)) set_target(kernel, world, anims, id, controlled);
    }
}

// @orig 1148:3B96 (No Remorse 1138:2D72)
void wake_npcs_now_and_then(Kernel& kernel, World& world, const AnimDat& anims) {
    GameState& s = world.state;
    if (s.npcs_woken && kernel.ticks() - s.last_wake < 240) return;
    s.npcs_woken = true;
    s.last_wake = kernel.ticks();
    wake_npcs_on_screen(kernel, world, anims);
}

void fidget(Kernel& kernel, World& world, const AnimDat& anims, ObjId npc) {
    if (actor_busy(kernel, npc)) return;
    do_anim(kernel, world, anims, npc, rnd(world, 2) == 0 ? 0x1E : 0x1F, kDirCurrent, 0, false);
    do_anim(kernel, world, anims, npc, 0, kDirCurrent, 1, false);
}

// The activities without a combat process (set_activity, attack.cpp, for
// the changes of No Regret).
// @orig 10F8:33B5
Pid start_activity_process(Kernel& kernel, World& world, const AnimDat& anims, ObjId npc, std::uint16_t activity) {
    std::unique_ptr<Process> p;
    switch (activity) {
    case 2: p = std::make_unique<LoiterProcess>(world, anims, npc, std::uint16_t{0xFFFF}); break;
    case 3: p = std::make_unique<WalkProcess>(world, anims, npc); break;
    case 4: case 6: return 0;
    case 7: p = std::make_unique<SurrenderProcess>(world, anims, npc); break;
    case 8: p = std::make_unique<GuardProcess>(world, anims, npc); break;
    default: p = std::make_unique<StandProcess>(world, anims, npc); break;  // 1 and others
    }
    return kernel.add(std::move(p));
}

}  // namespace crusader
