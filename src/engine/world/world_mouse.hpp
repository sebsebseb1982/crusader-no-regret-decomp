#pragma once
// The mouse in play: the game view's mouse methods (WORLD.C, vtable DS:6243:
// +4 idle 13E8:307F, +8 left button 2B74, +0xC right button 2DAD, +0x10
// motion 2A13). The cursor stays hidden; the mouse drives the controlled
// NPC by sending key events to the Controller (DS:284E +0x28), as the
// keyboard does (CombatControl::handle_key):
//
// - Motion (out of the item selection): the mouse goes back to (320, 200)
//   and dx = 320 - x. Unless the player is busy (DS:7E10), the rate is |dx|
//   >> (3 - speed) in a combat stance (1138:0070), else |dx| >> (7 - 2 x
//   speed), speed being the options' mouse speed (DS:6001, 0..3). A non-zero
//   rate sends a press of the left arrow (dx > 0) or the right one (E0,
//   count 1, the BIOS shift state), flagged as coming from the mouse
//   (DS:5FF6) with the rate (DS:5FF8): the crosshair turns by rate / 2
//   degrees (at most 20), a walk turns at most every 60 / (3 speed + 3)
//   ticks. Then, walking or firing, the idle method runs.
// - Right button: the press (after the double-click delay, mouse_queue.hpp)
//   starts walking (view flag 1); in a combat stance with a button held, the
//   NPC first stands up if kneeling (0x29) and holsters (0xB). The release
//   ends it with a release of the up arrow (E0). A double click, when the
//   NPC is not busy, holsters in a combat stance (standing up first), else
//   draws (7).
// - Left button: a press or a double click starts firing (view flag 8); the
//   release sends a release of Space once and ends it. In the item
//   selection, a press picks or uses the selected item (selection_click).
// - Idle (every input pass with no event): walking, when the NPC is not
//   busy, a press of the up arrow (E0; Alt held in a combat stance, so 0x24);
//   firing, a press of Space (count 1) when the controlled NPC is not busy
//   (SPRITE_1150_05E8).
//
// The player in stasis (DS:7E10, being the controlled NPC) ignores the
// buttons, except the release that ends a walk.
//
// Not used in normal play: the debug "hack mover" (DS:5FFA, H with cheats),
// view flags 2, 4 and 0x40, and the item under the mouse noted on a left
// press (DS:8C3D) for it; the "both buttons" event (+0x24 does nothing).

#include "core/anim_dat.hpp"
#include "engine/input/keyboard.hpp"
#include "engine/input/mouse_queue.hpp"
#include "engine/kernel.hpp"
#include "engine/world/world.hpp"

#include <functional>

namespace crusader {

class WorldMouse {
public:
    static constexpr int kCentreX = 320, kCentreY = 200;
    using SendKey = std::function<void(const GameKeyEvent&)>;

    WorldMouse(Kernel& kernel, World& world, const AnimDat& anims, const Keyboard& keyboard, SendKey send)
        : kernel_(kernel), world_(world), anims_(anims), keyboard_(keyboard), send_(std::move(send)) {}

    // An event of the mouse queue. Returns true when the mouse goes back to
    // (kCentreX, kCentreY).
    bool event(const MouseInput& e);
    // The idle event (+4) at the mouse position.
    void idle();

    bool walking() const { return walking_; }
    bool firing() const { return firing_; }

private:
    bool motion(const MouseInput& e);
    void left(const MouseInput& e);
    void right(const MouseInput& e);
    void send_arrow(std::uint8_t scancode, bool pressed, std::uint16_t flags);
    void send_fire(bool pressed);
    bool player_held() const;

    Kernel& kernel_;
    World& world_;
    const AnimDat& anims_;
    const Keyboard& keyboard_;
    SendKey send_;
    bool walking_ = false;  // view flag 1
    bool firing_ = false;   // view flag 8
};

}  // namespace crusader
