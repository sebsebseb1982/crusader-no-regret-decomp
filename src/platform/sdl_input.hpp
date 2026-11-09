#pragma once
// SDL events -> the game's input events. Replaces the keyboard ISR
// (KEYBOARD.C / KDAEMON.C) and the int 33h mouse driver callback (MOUSECTL.C).
//
// The engine reasons in PC "set 1" scancodes with BIOS-style shift flags
// (KeyEvent, 12 bytes in the original), so SDL scancodes are translated back.

#include "engine/input/keyboard.hpp"

#include <cstdint>
#include <optional>

union SDL_Event;

namespace crusader {


struct KeyEvent {
    std::uint8_t ascii = 0;      // 0 when the key has no character
    std::uint8_t scancode = 0;   // PC set 1, without the 0x80 release bit
    std::uint16_t flags = 0;     // KeyFlags
    bool extended = false;       // E0-prefixed key (arrows, right Ctrl, keypad Enter...)
    bool pressed = true;

    bool shift() const { return flags & (kLeftShift | kRightShift); }
    bool ctrl() const { return flags & (kLeftCtrl | kRightCtrl); }
    bool alt() const { return flags & (kLeftAlt | kRightAlt); }
};

struct MouseEvent {
    enum class Kind : std::uint8_t { Move, ButtonDown, ButtonUp } kind = Kind::Move;
    int x = 0;  // game screen coordinates
    int y = 0;
    int dx = 0;  // motion since the last event (relative mode included)
    int dy = 0;
    int button = 0;  // 1 left, 2 right, 3 middle
};

// PC set-1 scancode for an SDL scancode; 0 if unmapped. `extended` is set for
// E0-prefixed keys.
std::uint8_t pc_scancode(int sdl_scancode, bool* extended = nullptr);

std::optional<KeyEvent> translate_key(const SDL_Event& ev);
std::optional<MouseEvent> translate_mouse(const SDL_Event& ev);

}  // namespace crusader
