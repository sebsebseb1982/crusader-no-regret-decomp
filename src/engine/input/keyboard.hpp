#pragma once
// Keyboard state and key events as the game sees them (KERNEL.C keyboard
// interrupt handler at 11D8:3574, KeyDaemon in KDAEMON.C).
//
// The interrupt handler keeps, per scancode (normal and E0-extended tables),
// the time of the press and a count, and queues a 12-byte event. A make code
// for a key already down is ignored: the BIOS typematic repeat is not used.
// Instead the KeyDaemon, a 45 Hz timed process, re-sends every key held for
// at least the repeat delay (10 timer ticks; 20 in some screens) with an
// increasing count, once per tick.

#include <array>
#include <cstdint>
#include <functional>

namespace crusader {

// BIOS shift-state layout used by the original KeyEvent (+2).
enum KeyFlags : std::uint16_t {
    kRightShift = 0x0001,
    kLeftShift = 0x0002,
    kCtrl = 0x0004,  // either Ctrl (BIOS 0:417 bit 2), tested by DISPATCH.C
    kAlt = 0x0008,   // either Alt (bit 3)
    kNumLock = 0x0020,
    kCapsLock = 0x0040,
    kLeftCtrl = 0x0100,
    kLeftAlt = 0x0200,
    kRightCtrl = 0x0400,
    kRightAlt = 0x0800,
};

// KeyEvent (12 bytes in the original).
struct GameKeyEvent {
    std::uint8_t ascii = 0;     // +0
    std::uint8_t scancode = 0;  // +1, PC set 1 without the release bit
    std::uint16_t flags = 0;    // +2, BIOS shift state (0x20 Num Lock, 0x40 Caps Lock, see KeyFlags)
    std::uint32_t time = 0;     // +4, timer ticks
    bool pressed = false;       // +8
    bool extended = false;      // +9, E0 prefix
    std::uint16_t count = 0;    // +10, 1 for the press, then 2, 3... for repeats; 0 on release
};

class Keyboard {
public:
    static constexpr std::uint32_t kRepeatDelay = 10;  // DS:2DC2, timer ticks
    static constexpr std::uint32_t kDaemonRate = 45;   // KeyDaemon, Hz

    using Handler = std::function<void(const GameKeyEvent&)>;
    void set_handler(Handler h) { handler_ = std::move(h); }

    // Interrupt handler: a key went down or up at timer tick `now`.
    void key(std::uint8_t scancode, bool extended, bool pressed, std::uint16_t flags, std::uint8_t ascii,
             std::uint32_t now);
    // KeyDaemon tick: repeats held keys.
    void repeat(std::uint32_t now);

    // KERNEL.C 39B9: press count of a key, 0 when up.
    std::uint16_t down(std::uint8_t scancode, bool extended) const;
    std::uint16_t flags() const { return flags_; }
    // DS:322A: the number of keys held, counted by the interrupt handler
    // (presses minus releases, never below 0); Ultima8Engine::I_moveKeyDownRecently
    // (0x096, 10A8:4340) returns it, whatever the keys.
    std::uint16_t held() const { return held_; }
    void set_repeat_delay(std::uint32_t ticks) { repeat_delay_ = ticks; }
    // KERNEL.C 33E3 / 1FC1: forgets every key held (and the queue).
    void reset() {
        keys_ = {};
        held_ = 0;
    }

private:
    struct KeyState {
        std::uint32_t time = 0;
        std::uint16_t count = 0;
    };
    void send(const GameKeyEvent& e) {
        if (handler_) handler_(e);
    }

    std::array<std::array<KeyState, 128>, 2> keys_{};  // [extended][scancode]
    std::uint16_t flags_ = 0;
    std::uint16_t held_ = 0;
    std::uint32_t repeat_delay_ = kRepeatDelay;
    Handler handler_;
};

}  // namespace crusader
