#include "engine/world/combat_control.hpp"

#include "engine/world/auto_firer.hpp"

#include "core/dtable.hpp"
#include "engine/audio/audio.hpp"
#include "engine/world/activity.hpp"
#include "engine/world/aim_mode.hpp"
#include "engine/world/actor_anim.hpp"
#include "engine/world/attack.hpp"
#include "engine/world/auto_firer.hpp"
#include "engine/world/cheats.hpp"
#include "engine/world/player_commands.hpp"

namespace crusader {

namespace {

constexpr std::uint8_t kKeySpace = 0x39;
constexpr std::uint8_t kKeyInsert = 0x52;  // keypad 0 without E0
constexpr std::uint16_t kShotgun = 0x332;
constexpr std::uint16_t kActionRaise = 7;
constexpr std::uint16_t kActionShoot = 8;
constexpr std::uint16_t kActionReload = 0xA;
constexpr std::uint16_t kActionKneelShoot = 0x2A;
constexpr std::uint32_t kReloadWait = 15;
constexpr std::uint32_t kEmptyWait = 20;

bool is_fire_key(const GameKeyEvent& e) {
    return e.scancode == kKeySpace || (e.scancode == kKeyInsert && !e.extended);
}

std::uint16_t last_anim(const World& w, ObjId npc) {
    const auto r = w.npc_record(npc);
    return r.empty() ? 0 : static_cast<std::uint16_t>(r[npc::kLastAnim] | r[npc::kLastAnim + 1] << 8);
}

bool kneeling(const World& w, ObjId npc) {  // Actor::I_isKneeling, record +0x13 bit 2
    const auto r = w.npc_record(npc);
    return !r.empty() && (r[npc::kFlags13] & 0x04);
}

// NPC_10f0_0122.
bool on_map_40(const World& w, ObjId npc) {
    const Item* it = w.get(npc);
    return it && it->map_number == 40;
}

// NPC_10f0_395e: first item of the player's 30 equipment slots with this shape.
// @orig 10F8:4056
ObjId find_equipped(const World& w, std::uint16_t shape) {
    for (int slot = 0; slot < w.equip_slots(); ++slot) {
        const ObjId id = w.npc_equip(1, slot);
        if (const Item* it = id ? w.get(id) : nullptr; it && it->shape == shape) return id;
    }
    return 0;
}

// NPC_10f8_011a (No Regret): the action of the NPC's animation when a shot
// may cut it short: one of a list (standing, kneeling, turning, aiming,
// shooting...), once 70% of its frames have played (30% for 0x28 and 0x29),
// or at once when it has one frame; 0x40 otherwise. The frame count is the
// action's (the animation process +0x5A), the frame the NPC record's (+0x10).
// @orig 10F8:011A
std::uint16_t interruptible_action(const Kernel& kernel, const World& w, const AnimDat& anims, ObjId npc) {
    const ActorAnimProcess* anim = current_anim(kernel, npc);
    const Item* item = w.get(npc);
    const auto record = w.npc_record(npc);
    if (!anim || !item || record.empty()) return 0x40;
    const std::uint16_t action = anim->action();
    switch (action) {
    case 0x3D: case 0x3E: case 0x17: case 0x18: case 0: case 4: case 0x25: case 5: case 7: case 0xC: case 8:
    case 0xD: case 0xB: case 0x10: case 0x27: case 0x19: case 0x1C: case 0x1D: case 0x28: case 0x29: case 0x2A:
    case 0x2B: case 0x32: case 0x36: case 0x3A: case 0x2E: case 0x2F:
        break;
    default:
        return 0x40;
    }
    const AnimAction* a = anims.action(item->shape, action);
    const std::uint32_t frames = a ? static_cast<std::uint8_t>(a->frame_count()) : 0;
    const std::uint32_t needed = frames * (action == 0x28 || action == 0x29 ? 300u : 700u);
    if (needed == 700 || needed <= static_cast<std::uint32_t>(record[npc::kAnimFrame]) * 1000) return action;
    return 0x40;
}

}  // namespace

// @orig 1148:0121
CombatControl::CombatControl(World& world, const AnimDat& anims, const Keyboard& keyboard, AvatarMover& mover)
    : world_(world), anims_(anims), keyboard_(keyboard), mover_(mover) {
    set_type(kType);
    set_runs_at_any_level(true);
    set_rate(60);
}

// @orig 10F8:4117
int CombatControl::rounds(const World& world, ObjId npc) {
    const Item* weapon = world.get(world.active_weapon(npc));
    if (!weapon) return 0;
    if (dtable::ammo_shape(weapon->shape) == 0) {
        const int cost = dtable::weapon(weapon->shape).energy_per_shot();
        return cost ? world.npc_mana(npc) / cost : 1;
    }
    return weapon->quality;
}

// @orig 10F8:41AA
void CombatControl::use_round(ObjId npc) {
    Item* weapon = world_.get(world_.active_weapon(npc));
    if (!weapon) return;
    if (dtable::ammo_shape(weapon->shape) == 0) {
        // NPC_10f0_379a: energy - cost, not below 0.
        const int cost = dtable::weapon(weapon->shape).energy_per_shot();
        const int energy = std::max(0, static_cast<int>(world_.npc_mana(npc)) - cost);
        auto r = world_.npc_record(npc);
        if (!r.empty()) {
            r[npc::kEnergy] = static_cast<std::uint8_t>(energy);
            r[npc::kEnergy + 1] = static_cast<std::uint8_t>(energy >> 8);
        }
        return;
    }
    weapon->quality = static_cast<std::uint16_t>(weapon->quality - 1);
}

void CombatControl::play_at(ObjId npc, int sfx) {
    if (sfx == 0) return;
    if (Audio* audio = world_.audio()) audio->sources.retrigger(world_, sfx, npc);
}

// @orig 10F8:429B
void CombatControl::reload(ObjId npc) {
    if (rounds(world_, npc) != 0) return;
    Item* weapon = world_.get(world_.active_weapon(npc));
    if (!weapon) return;
    const std::uint16_t ammo = dtable::ammo_shape(weapon->shape);
    if (ammo == 0) return;
    Item* clip = world_.get(find_equipped(world_, ammo));
    if (!clip) return;
    weapon->quality = dtable::clip_size(weapon->shape);
    clip->quality = static_cast<std::uint16_t>(clip->quality - 1);
    if (clip->quality == 0) world_.destroy(clip->id);
    play_at(npc, dtable::weapon(weapon->shape).reload_sfx());
    if (npc == 1 && !kneeling(world_, npc)) do_anim(kernel(), world_, anims_, npc, kActionReload, kDirCurrent, 10000, true);
}

void CombatControl::sleep_ticks(std::uint32_t ticks) {
    asleep_ = true;
    wake_at_ = kernel().ticks() + ticks;
}

// No Regret (1148:2F4C): a busy NPC may still shoot when its weapon has
// rounds, its last action is not a shot (8, 0xD, 0x1A, 0x2A, 0x2B, 0x32,
// 0x36) and its animation may be cut (interruptible_action): its
// animations then end. No Remorse never shoots while busy.
// @orig 1148:2F4C
bool CombatControl::fire_interrupts(ObjId npc) {
    if (world_.remorse() || rounds(world_, npc) == 0) return false;
    switch (last_anim(world_, npc)) {
    case 8: case 0xD: case 0x1A: case 0x2A: case 0x2B: case 0x32: case 0x36:
        return false;
    default:
        break;
    }
    if (interruptible_action(kernel(), world_, anims_, npc) == 0x40) return false;
    kernel().fail_processes(npc, ActorAnimProcess::kType, 1);
    return true;
}

bool CombatControl::fire_key_down() const {
    return keyboard_.down(kKeySpace, false) != 0 || keyboard_.down(kKeyInsert, false) != 0;
}

// No Regret (1148:2B65): nothing for a frozen NPC (shape 0x5D6); a shot
// may cut the NPC's animation short (fire_interrupts).
// @orig 1138:220A (No Remorse) 1148:2AB9
void CombatControl::handle_key(const GameKeyEvent& e) {
    const ObjId npc = world_.state.controlled_npc;
    if (npc == 0 || world_.is_dead(npc)) return;
    if (const Item* item = world_.get(npc); !world_.remorse() && item && item->shape == 0x5D6) return;
    if (npc == 1 && world_.state.avatar_in_stasis) return;  // DS:7E10
    if (firing_ && is_fire_key(e) && e.pressed) return;
    const bool used = mover_.handle_key(e);
    if (!used && e.pressed) {
        if (keypad_command(e)) return;
        if (!is_fire_key(e) || on_map_40(world_, npc)) return;
        if (e.count == 1) shot_since_press_ = false;
        if (actor_busy(kernel(), npc) && !fire_interrupts(npc)) return;
        if (in_combat_stance(world_, npc)) fire(true);
        else do_anim(kernel(), world_, anims_, npc, kActionRaise, kDirCurrent);
        return;
    }
    if (on_map_40(world_, npc) || !is_fire_key(e)) return;
    fire(false);
}

// No Regret (1148:2DFD): Backspace takes a picture once the PIX code is
// typed (cheats.hpp).
// @orig 1148:2AB9 1008:0D71 1008:0EB5
bool CombatControl::keypad_command(const GameKeyEvent& e) {
    if (e.scancode == 0x0F || e.scancode == 0x24) {  // Tab, J: at every repeat
        jump(kernel(), world_, anims_);
        return true;
    }
    if (!world_.remorse() && e.scancode == 0x0E) {  // Backspace
        if (world_.state.cheats.pictures && world_.hooks().take_picture) world_.hooks().take_picture();
        return true;
    }
    if (e.count != 1) return false;
    const bool player = world_.state.controlled_npc == 1;  // DS:285A
    switch (e.scancode) {
    case 0x37:  // keypad *
        if (!e.extended) next_weapon(kernel(), world_, anims_);
        return true;
    case 0x4A:  // keypad -
        if (player) next_item(world_);
        return true;
    case 0x4E:  // keypad +: an 's' for the game view (selection mode)
        if (player) player_command(kernel(), world_, anims_, 's');
        return true;
    case 0x53:  // keypad Del: a 'u' for the game view (World_on_key)
        if (player && !e.extended) player_command(kernel(), world_, anims_, 'u');
        return true;
    default:
        return false;
    }
}

// No Regret (1148:30F1): nothing while the AutoFirer runs (DS:302B) with
// no round left; robots of shapes 0x1CD, 0x4C8, 0x5E6 and 0x5BA shoot with
// no weapon; the player out of clips with the weapon 0x334 at difficulty 1
// or 2 gets a full clip of 0x33F (1148:01FB) and reloads.
// @orig 1148:30F1
bool CombatControl::fire(bool explicit_call) {
    const ObjId npc = world_.state.controlled_npc;
    const ObjId weapon_id = world_.active_weapon(npc);
    world_.state.fire_held = explicit_call;  // DS:28CD, read by the firing frame (AutoFirer)
    if (!world_.remorse() && world_.state.auto_firer != 0 && rounds(world_, npc) == 0) return false;
    if (burst_left_ != 0 && !single_) return false;
    if (actor_busy(kernel(), npc) && explicit_call) return false;

    bool fired = false;
    const Item* weapon = world_.get(weapon_id);
    const std::uint16_t weapon_shape = weapon ? weapon->shape : 0;
    const Item* shooter = world_.get(npc);
    const bool built_in = !world_.remorse() && shooter &&
                          (shooter->shape == 0x1CD || shooter->shape == 0x4C8 || shooter->shape == 0x5E6 ||
                           shooter->shape == 0x5BA);
    if (wanted_ && (explicit_call || !shot_since_press_) && (weapon || built_in)) {
        if (npc == 1 && rounds(world_, npc) == 0) {
            const bool clip = find_equipped(world_, dtable::ammo_shape(weapon_shape)) != 0;
            const bool refill = !clip && !world_.remorse() && weapon_shape == 0x334 &&
                                (world_.state.difficulty == 1 || world_.state.difficulty == 2);
            if (refill) give_ammo(world_, 0x33F);
            if (clip || refill) {
                wanted_ = firing_ = false;  // CRUSADER_1138_2fd4
                single_ = false;
                burst_left_ = 0;
                timer_on_ = false;
                if (weapon_shape == kShotgun) cocked_ = true;
                reload(npc);
                firing_ = true;
                sleep_ticks(kReloadWait);
            } else {
                wanted_ = false;
                play_at(npc, 0x2A);  // click
                firing_ = true;
                sleep_ticks(kEmptyWait);
            }
        } else if (npc == 1 && !kneeling(world_, npc) && weapon_shape == kShotgun && !cocked_) {
            play_at(npc, dtable::weapon(weapon_shape).reload_sfx());
            do_anim(kernel(), world_, anims_, npc, kActionReload, kDirCurrent, 10000, true);
            cocked_ = true;
        } else {
            if (weapon_shape == kShotgun) cocked_ = false;
            shot_since_press_ = true;
            const auto& rec = dtable::weapon(weapon_shape);
            const std::uint16_t action = kneeling(world_, npc) ? kActionKneelShoot : kActionShoot;
            if (do_anim(kernel(), world_, anims_, npc, action, kDirCurrent) != 0) {
                if (npc == 1) use_round(npc);
                // CRUSADER_1138_2d72 / 2daf: the NPCs on screen wake up, at
                // most once every 240 ticks.
                if (!woke_ || kernel().ticks() - last_wake_ >= 240) {
                    woke_ = true;
                    last_wake_ = kernel().ticks();
                    wake_npcs_on_screen(kernel(), world_, anims_);
                }
                firing_ = true;
                sleep_ticks(rec.shot_delay());
                if (single_) {
                    --burst_left_;
                } else if (rec.burst() >= 2) {
                    burst_left_ = rec.burst() - 1;  // CRUSADER_1138_01a5
                    timer_on_ = true;
                }
                fired = true;
            }
        }
    }
    if (single_) {
        if (burst_left_ == 0) {
            timer_on_ = false;
            wanted_ = fire_key_down();
        }
    } else if (burst_left_ == 0) {
        wanted_ = explicit_call;
    }
    return fired;
}

// @orig 1148:3DC3
void CombatControl::reset_fire() {
    wanted_ = firing_ = single_ = false;
    burst_left_ = 0;
    timer_on_ = false;
    stop_auto_firer(kernel(), world_);
}

void CombatControl::run() {
    if (!asleep_) refresh_crosshair(world_, anims_);
}

// @orig 1148:08D9
void CombatControl::tick() {
    if (asleep_) {
        if (kernel().ticks() < wake_at_) return;
        asleep_ = false;
        firing_ = false;  // message 4
    }
    if (!timer_on_) return;
    // Message 2 (CRUSADER_1138_226f): one shot of the burst.
    single_ = true;
    const ObjId npc = world_.state.controlled_npc;
    if (npc != 0 && !world_.is_dead(npc)) fire(true);
    single_ = false;
}

}  // namespace crusader
