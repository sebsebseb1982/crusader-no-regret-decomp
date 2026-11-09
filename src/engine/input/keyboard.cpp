#include "engine/input/keyboard.hpp"

namespace crusader {

// @orig 11D8:3574
void Keyboard::key(std::uint8_t scancode, bool extended, bool pressed, std::uint16_t flags, std::uint8_t ascii,
                   std::uint32_t now) {
    scancode &= 0x7F;
    flags_ = flags;
    KeyState& k = keys_[extended ? 1 : 0][scancode];
    if (pressed) {
        if (k.count != 0) return;  // typematic repeat: ignored
        k.count = 1;
        k.time = now;
        ++held_;
    } else {
        k = {};
        if (held_ != 0) --held_;
    }
    send({ascii, scancode, flags, now, pressed, extended, k.count});
}

// @orig 11C0:0129
void Keyboard::repeat(std::uint32_t now) {
    // KDAEMON.C tick (11C0:0129): every key held since `repeat_delay_` ticks
    // or more gets one more event, normal keys first, then extended ones.
    for (int ext = 0; ext < 2; ++ext) {
        for (std::uint8_t sc = 0; sc < 128; ++sc) {
            KeyState& k = keys_[ext][sc];
            if (k.count == 0 || k.time + repeat_delay_ > now) continue;
            ++k.count;
            send({0, sc, flags_, now, true, ext != 0, k.count});
        }
    }
}

std::uint16_t Keyboard::down(std::uint8_t scancode, bool extended) const {
    return keys_[extended ? 1 : 0][scancode & 0x7F].count;
}

}  // namespace crusader
