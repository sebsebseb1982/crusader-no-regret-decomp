#include "engine/world/world_mouse.hpp"

#include "engine/world/actor_anim.hpp"
#include "engine/world/attack.hpp"
#include "engine/world/avatar_mover.hpp"
#include "engine/world/item_selection.hpp"

#include <cstdlib>

namespace crusader {

namespace {

// Scancodes (PC set 1).
constexpr std::uint8_t kKeySpace = 0x39;
constexpr std::uint8_t kKeyUp = 0x48;
constexpr std::uint8_t kKeyLeft = 0x4B;
constexpr std::uint8_t kKeyRight = 0x4D;

constexpr std::uint16_t kActionDraw = 7;
constexpr std::uint16_t kActionHolster = 0xB;
constexpr std::uint16_t kActionStandUp = 0x29;

bool kneeling(const World& world, ObjId id) {  // Actor::I_isKneeling
    const auto r = world.npc_record(id);
    return !r.empty() && (r[npc::kFlags13] & 0x04);
}

}  // namespace

// The player in stasis while controlled (10A8:00C1 on DS:7E0E, DS:7E10).
bool WorldMouse::player_held() const {
    return world_.state.controlled_npc == 1 && world_.state.avatar_in_stasis;
}

// @orig 13E8:2A13 13E8:2B74 13E8:2DAD
bool WorldMouse::event(const MouseInput& e) {
    switch (e.kind()) {
    case MouseInput::kMotionKind: return motion(e);
    case MouseInput::kLeft: left(e); return false;
    case MouseInput::kRight: right(e); return false;
    default: return false;  // both buttons: +0x24 does nothing in the view
    }
}

// @orig 13E8:2A13
bool WorldMouse::motion(const MouseInput& e) {
    // The mouse goes back to the centre, out of the item selection.
    const bool warp = !world_.state.selection.active;
    const int dx = warp ? kCentreX - e.x : 0;
    if (world_.state.avatar_in_stasis) return warp;
    GameState::MouseTurn& mouse = world_.state.mouse_turn;
    const int speed = mouse.speed;
    mouse.rate = stance_not_moving(world_, world_.state.controlled_npc) ? std::abs(dx) >> (3 - speed)
                                                                        : std::abs(dx) >> (7 - 2 * speed);
    if (mouse.rate != 0) {
        mouse.active = true;
        send_arrow(dx > 0 ? kKeyLeft : kKeyRight, true, keyboard_.flags());
        mouse.active = false;
    }
    if (walking_ || firing_) idle();
    return warp;
}

// @orig 13E8:2B74
void WorldMouse::left(const MouseInput& e) {
    if (world_.state.selection.active) {
        if (e.type == MouseInput::kPress) selection_click(world_);
        return;
    }
    if (player_held()) return;
    if (e.type != MouseInput::kRelease) {
        firing_ = true;
        idle();
    } else if (firing_) {
        send_fire(false);
        firing_ = false;
    }
}

// @orig 13E8:2DAD
void WorldMouse::right(const MouseInput& e) {
    if (world_.state.selection.active) return;  // view flags 0x180
    const ObjId npc = world_.state.controlled_npc;
    if (e.type == MouseInput::kRelease) {
        if (!walking_) return;
        walking_ = false;
        send_arrow(kKeyUp, false, keyboard_.flags());
        return;
    }
    if (player_held()) return;
    if (e.type == MouseInput::kPress) {
        walking_ = true;
        if (stance_not_moving(world_, npc) && (e.buttons & 3) != 0) {
            if (kneeling(world_, npc)) do_anim(kernel_, world_, anims_, npc, kActionStandUp, kDirCurrent, 0);
            do_anim(kernel_, world_, anims_, npc, kActionHolster, kDirCurrent, 1);
        }
        // The original then compares the NPC's direction with itself and
        // turns it when they differ: never.
        idle();
        return;
    }
    // A double click.
    if (actor_busy(kernel_, npc)) return;
    if (stance_not_moving(world_, npc)) {
        if (kneeling(world_, npc)) do_anim(kernel_, world_, anims_, npc, kActionStandUp, kDirCurrent, 0);
        do_anim(kernel_, world_, anims_, npc, kActionHolster, kDirCurrent, 1);
    } else {
        do_anim(kernel_, world_, anims_, npc, kActionDraw, kDirCurrent, 0);
    }
}

// @orig 13E8:307F
void WorldMouse::idle() {
    if (player_held()) return;
    // The item selection's part of this method is selection_tick.
    if (world_.state.selection.active) return;
    const ObjId npc = world_.state.controlled_npc;
    if (walking_ && !actor_busy(kernel_, npc) &&
        turn_to(kernel_, world_, anims_, npc, world_.npc_direction(npc), false, false) == 0) {
        send_arrow(kKeyUp, true, stance_not_moving(world_, npc) ? kLeftAlt : keyboard_.flags());
    }
    if (firing_) send_fire(true);
}

// The key events built by the view: E0 arrows, count 1, time 0.
// @orig 13E8:2A13 13E8:2DAD 13E8:307F; the key events they build
void WorldMouse::send_arrow(std::uint8_t scancode, bool pressed, std::uint16_t flags) {
    GameKeyEvent k;
    k.scancode = scancode;
    k.flags = flags;
    k.pressed = pressed;
    k.extended = true;
    k.count = 1;
    send_(k);
}

// @orig 1150:05E8
void WorldMouse::send_fire(bool pressed) {
    if (actor_busy(kernel_, world_.state.controlled_npc)) return;  // DS:285A
    GameKeyEvent k;
    k.ascii = ' ';
    k.scancode = kKeySpace;
    k.pressed = pressed;
    k.count = 1;
    send_(k);
}

}  // namespace crusader
