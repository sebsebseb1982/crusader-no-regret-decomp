#include "engine/world/teleport.hpp"

#include "engine/audio/audio.hpp"
#include "engine/world/aim_mode.hpp"
#include "engine/world/gravity.hpp"

#include <array>
#include <cstdio>

namespace crusader {

namespace {

constexpr ObjId kPlayer = 1;
constexpr std::uint16_t kPlayerAnimType = 0xF0;
constexpr std::uint8_t kFamilyEgg = 8;

// DS:0F89: set while a failed teleport takes the player back.
bool g_returning = false;

// Process DS:1006 (1098:0000 / 0047): raises the busy flag while it lives;
// it waits for the teleport, then ends at once, putting the flag back.
// @orig 10A0:0000 10A0:0047
class BusyUntilTeleported : public Process {
public:
    explicit BusyUntilTeleported(World& world) : world_(world), was_busy_(world.state.avatar_in_stasis) {
        world_.state.avatar_in_stasis = true;
    }

protected:
    void run() override { terminate(); }
    void on_terminate() override {
        if (!was_busy_) world_.state.avatar_in_stasis = false;
    }

private:
    World& world_;
    bool was_busy_;
};

// The player to teleport egg `egg_number` of `map` (loading it first when it
// is not the current one); without that egg, back where he was. A mission's
// start (egg 0x1E) is autosaved (port addition). No Regret's SwitchMap does
// the same at once (CDLOAD_1030_0000), there with no message; its other case
// (DS:0ADC clear, no player: the camera moves instead) does not occur here.
// @orig 10A0:011F 1030:0000
void teleport_to_egg_now(Kernel& kernel, World& world, int map, std::uint16_t egg_number) {
    Item* player = world.get(kPlayer);
    if (!player) return;
    const std::int32_t old_x = player->x, old_y = player->y, old_z = player->z;
    const int old_map = player->map_number;
    const bool same_map = map == world.current_map();
    if (!same_map) world.teleport(kPlayer, 0, 0, 0, map);

    const ObjId egg = find_teleport_egg(world, egg_number);
    if (egg == 0) {
        if (!same_map) {
            g_returning = true;
            world.teleport(kPlayer, old_x, old_y, old_z, old_map);
            g_returning = false;
        }
        if (world.remorse()) std::fprintf(stderr, "No destination for this egg! (map %d, egg %u)\n", map, egg_number);
        return;
    }
    const Item& e = *world.get(egg);
    const std::int32_t x = e.x, y = e.y, z = e.z;
    world.teleport(kPlayer, x, y, z, map);
    player = world.get(kPlayer);
    if (!(support_mask(world, player->shape, x, y, z, true, kPlayer) & 0x10)) fall(kernel, world, kPlayer);
    if (egg_number == kTeleportStartEgg && world.hooks().autosave_soon) world.hooks().autosave_soon();
}

// @orig 10A0:011F
class TeleportToEgg : public Process {
public:
    TeleportToEgg(Kernel& kernel, World& world, int map, std::uint16_t egg)
        : kernel_(kernel), world_(world), map_(map), egg_(egg) {}

protected:
    void run() override {
        if (!g_returning) teleport_to_egg_now(kernel_, world_, map_, egg_);
        terminate();
    }

private:
    Kernel& kernel_;
    World& world_;
    int map_;
    std::uint16_t egg_;
};

// SHAPHAND_1098_01f4: the player's animations end; with `wait`, the busy
// flag is up until the teleport is over.
// @orig 10A0:01EA
Pid spawn_teleport(Kernel& kernel, World& world, int map, std::uint16_t egg, bool wait) {
    kernel.fail_processes(kPlayer, kPlayerAnimType, 1);
    Process& teleport = kernel.spawn<TeleportToEgg>(kernel, world, map, egg);
    if (wait && !g_returning) kernel.spawn<BusyUntilTeleported>(world).wait_for(teleport.pid());
    return teleport.pid();
}

// @orig 10A0:0681 10A0:070E
// The same as No Remorse 1098:0718 (two more locals, unused).
class EggHatcher : public Process {
public:
    static constexpr std::uint16_t kType = 0x20F;

    EggHatcher(Kernel& kernel, World& world, ObjId egg) : kernel_(kernel), world_(world) {
        set_owner(egg);
        set_type(kType);
    }

protected:
    void run() override {
        const Item* egg = world_.get(owner());
        const Item* player = world_.get(kPlayer);
        if (!egg || !player || world_.family_of(egg->shape) == 7) return;
        const ShapeInfo* info = world_.shape_info(player->shape);
        const int foot_x = info ? info->footpad_x : 0, foot_y = info ? info->footpad_y : 0;
        const int range_x = egg->npc_number >> 4, range_y = egg->npc_number & 0xF;
        auto u16 = [](std::int32_t v) { return static_cast<std::uint16_t>(v); };
        const std::uint16_t px = u16(player->x), py = u16(player->y);
        bool inside = !(px < u16(egg->x - range_x * 32) || u16(egg->x + range_x * 32) < u16(px - foot_x * 16 + 1) ||
                        py < u16(egg->y - range_y * 32) || u16(egg->y + range_y * 32) < u16(py - foot_y * 16 + 1));
        const int pz = player->z & 0xFF, ez = egg->z & 0xFF;
        if (inside && (ez + 0x30 < pz || pz < ez - 0x30)) inside = false;
        if (!hatched_) {
            if (inside) {
                hatched_ = true;
                hatch(*egg);
            }
        } else if (!inside) {
            hatched_ = false;
            if (world_.hooks().start_event) world_.hooks().start_event(owner(), 0x15);
        }
    }

private:
    // @orig 10A0:05F3
    void hatch(const Item& egg) {
        if (world_.family_of(egg.shape) == 8) {
            spawn_teleport(kernel_, world_, egg.map_number, egg.quality & 0xFF, (egg.quality >> 8) != 0);
        } else if (world_.hooks().start_event) {
            world_.hooks().start_event(egg.id, 7);
        }
    }

    Kernel& kernel_;
    World& world_;
    bool hatched_ = false;
};

// Process DS:0566 (1028:0477).
//
// No Regret (1030:0C0B) changes missions instead. After the same reset
// (1030:0B6F, which also shows a loading picture, gump shape 7, on a black
// screen in the game palette, the colour cycling running: only the palette
// is reproduced), only the player and his equipment stay
// (World::restart_with_player). He then goes to egg 0x1E at once
// (CDLOAD_1030_0000) and, on a map up to 200, the screen goes black
// (PaletteFaderProcess::I_jumpToAllBlack). Not reproduced: SPMGR_11c8_0d39
// at the end (the status bar, which the port redraws whole).
// @orig 1030:0BCA 1030:0C0B 1148:3FB5
class SwitchMap : public Process {
public:
    SwitchMap(Kernel& kernel, World& world, int map) : kernel_(kernel), world_(world), map_(map) {}

protected:
    void run() override {
        // The colour cycling pauses; GAMEPAL_1028_03f6, this process
        // included (not the Controller, reset instead: 1138:3181); then
        // the teleport.
        world_.state.cycle_paused = true;
        kernel_.fail_all();
        leave_aim_mode(kernel_, world_);
        if (world_.hooks().reset_fire) world_.hooks().reset_fire();
        GameState::Aim& aim = world_.state.aim;
        aim.marker_pid = 0;
        aim.target = 0;
        aim.marker = -1;
        aim.marked_dir = 0x10;
        aim.turns = 0;
        if (Audio* audio = world_.audio()) {
            audio->stop_all();
            audio->sources.clear();
        }
        if (world_.remorse()) {
            start_teleport_to_egg(kernel_, world_, map_, kTeleportStartEgg, true, false);
            return;
        }
        GameState& s = world_.state;
        s.screen_palette = s.game_palette;
        s.cycle_paused = false;
        world_.restart_with_player();
        teleport_to_egg_now(kernel_, world_, map_, kTeleportStartEgg);
        if (map_ <= 200) {
            s.cycle_paused = true;
            s.screen_palette = Palette::filled(0, 0, 0);
        }
    }

private:
    Kernel& kernel_;
    World& world_;
    int map_;
};

}  // namespace

// @orig 10A0:0223
ObjId find_teleport_egg(const World& world, std::uint16_t egg) {
    const std::array<std::uint8_t, 18> script = {
        ':', '%', kFamilyEgg, 0, '=', '`', '%', 1, 0, '=', '&', '*', '%', 0xFF, 0, '&', '%', 0,
    };
    std::array<std::uint8_t, 24> code{};
    std::copy(script.begin(), script.end(), code.begin());
    code[17] = static_cast<std::uint8_t>(egg);
    code[18] = static_cast<std::uint8_t>(egg >> 8);
    code[19] = '=';
    code[20] = '&';
    code[21] = '$';
    // The whole map (AreaSearch_init with every bound negative), first match.
    for (ObjId id : world.rect_search(0, 0, 0x7FFF, 0x7FFF)) {
        if (world.matches_loop_script(*world.get(id), std::span(code).first(22))) return id;
    }
    return 0;
}

// @orig 10A0:0072 10A0:0192
Pid start_teleport_to_egg(Kernel& kernel, World& world, int map, std::uint16_t egg, bool wait, bool stop_sounds) {
    const Pid pid = spawn_teleport(kernel, world, map, egg, wait);
    // Only the explicit-map constructor stops the sounds (1098:011F); the
    // current-map one (1098:019C) leaves them playing.
    if (stop_sounds) {
        if (Audio* audio = world.audio()) {
            audio->sources.clear();
            audio->stop_all();
        }
    }
    return pid;
}

Pid start_switch_map(Kernel& kernel, World& world, int map) {
    return kernel.spawn<SwitchMap>(kernel, world, map).pid();
}

Pid start_egg_hatcher(Kernel& kernel, World& world, ObjId egg) {
    return kernel.spawn<EggHatcher>(kernel, world, egg).pid();
}

}  // namespace crusader
