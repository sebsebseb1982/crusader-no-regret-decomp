#pragma once
// Mouse events as the game sees them (MOUSECTL.C, MouseCtl_on_driver_event
// 1360:045D and MouseCtl_next_event 1360:0E1F; DISPATCH.C 1328:0935).
//
// MouseCtl is a 60 Hz timed process that also receives the int 33h driver
// callback. A button press is held back until it is known not to start a
// double click: it is sent 13 ticks later, followed by the release if one
// came meanwhile; a second press of the same button within those 13 ticks
// is sent at once as a double click instead (the first click and its
// release are dropped). Pressing the other button while a press is held
// back sends a single "both buttons" press (code 0x2A) and drops both.
// A release with no press held back is sent at once. Button events go into
// a queue of 10 (further ones are lost); motion is not queued: only the
// latest position is kept, and it comes after the queued events.
//
// DISPATCH turns the code into the gump event kind, which picks the gump's
// method (NEWGUMP 1368:0CB2): left button +8, right +0xC, motion +0x10,
// both +0x24. When a pass of the input dispatch (1328:0624) has no event
// at all, the gump gets an idle event (+4) at the current position.

#include <cstdint>
#include <deque>
#include <optional>

namespace crusader {

struct MouseInput {
    enum Type : std::uint8_t { kPress = 1, kDoubleClick = 2, kRelease = 3, kMotion = 4 };
    enum Code : std::uint8_t {
        kMove = 1,
        kLeftDown = 2,
        kLeftUp = 4,
        kRightDown = 8,
        kRightUp = 0x10,
        kBothDown = 0x2A,
        kBothUp = 0x54,
    };
    enum Kind : std::uint16_t { kLeft = 2, kRight = 4, kMotionKind = 8, kBoth = 0x100 };

    Type type = kMotion;      // gump event +0x0C (GumpEvent_is_press: 1)
    std::uint8_t code = kMove;
    std::uint16_t buttons = 0;  // +0x0E, driver button state: bit 0 left, bit 1 right
    int x = 0, y = 0;           // +2, +4

    Kind kind() const;
};

class MouseQueue {
public:
    static constexpr int kClickDelay = 13;  // ticks of the 60 Hz MouseCtl
    static constexpr std::size_t kCapacity = 10;

    // Driver callback; `buttons` is the state after the event.
    void move(int x, int y, std::uint16_t buttons);
    void press(bool right, int x, int y, std::uint16_t buttons);
    void release(bool right, int x, int y, std::uint16_t buttons);
    // MouseCtl's 60 Hz tick: held-back presses ripen.
    void tick();
    // MouseCtl_next_event: a queued event, else the latest motion.
    std::optional<MouseInput> next();
    void clear();

private:
    struct Held {
        MouseInput press;
        int ticks = 0;
        std::optional<MouseInput> release;
    };
    void send(const MouseInput& e);

    std::optional<Held> held_[2];  // left, right
    std::deque<MouseInput> queue_;
    std::optional<MouseInput> motion_;
};

}  // namespace crusader
