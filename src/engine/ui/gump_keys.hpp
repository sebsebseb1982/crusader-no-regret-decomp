#pragma once
// Key code handed to the gumps (DISPATCH_1328_0624), from a key event of the
// keyboard queue. With the BIOS shift state `f` (bits 0-1 Shift, 2 Ctrl, 3 Alt):
//   no modifier: the ASCII code, or 0x100 + scancode when it is 0
//   Alt:         0x200 + scancode
//   Ctrl:        0x400 + scancode (wins over Alt)
//   Shift:       the ASCII code, or 0x800 + scancode when it is 0 (wins over both)
// So the up arrow is 0x148, Escape 0x1B, Alt+X 0x22D. Releases send nothing.

#include "engine/input/keyboard.hpp"

#include <cstdint>

namespace crusader {

namespace gump_key {
constexpr std::uint16_t kEnter = 0x0D;
constexpr std::uint16_t kEscape = 0x1B;
constexpr std::uint16_t kUp = 0x148;
constexpr std::uint16_t kDown = 0x150;
constexpr std::uint16_t kLeft = 0x14B;
constexpr std::uint16_t kRight = 0x14D;
constexpr std::uint16_t kAltX = 0x22D;
constexpr std::uint16_t kF1 = 0x13B;
constexpr std::uint16_t kF2 = 0x13C;
constexpr std::uint16_t kF4 = 0x13E;
constexpr std::uint16_t kF5 = 0x13F;
constexpr std::uint16_t kF6 = 0x140;
constexpr std::uint16_t kF8 = 0x142;
constexpr std::uint16_t kF9 = 0x143;
}  // namespace gump_key

// 0 when the event gives no code (a release).
// @orig 1328:0624
inline std::uint16_t gump_key_code(const GameKeyEvent& e) {
    if (!e.pressed) return 0;
    const std::uint16_t mods = e.flags & 0x0F;
    if (mods == 0) return e.ascii ? e.ascii : static_cast<std::uint16_t>(0x100 | e.scancode);
    std::uint16_t code = 0;
    if (mods & kAlt) code = static_cast<std::uint16_t>(e.scancode + 0x200);
    if (mods & kCtrl) code = static_cast<std::uint16_t>(e.scancode + 0x400);
    if (mods & (kLeftShift | kRightShift)) code = e.ascii ? e.ascii : static_cast<std::uint16_t>(e.scancode + 0x800);
    return code;
}

}  // namespace crusader
