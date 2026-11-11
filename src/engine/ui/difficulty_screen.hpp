#pragma once
// The difficulty screen (IRGUMP_13d0_25a2, segment 13D0), opened by the main
// menu's "new game" button. Choosing a level starts a new game.
//
// Palette STATIC/DIFF.PAL; mouse cursor 0x26, the mouse put at (320, 400).
// Background (13D0:2B16): the screen cleared, GUMPS.FLX shape 0x49 frame 0 at
// (139, 76) and frame 1 at (331, 76); the US shape has a single frame, centred
// at y 76. Four buttons (13D0:2D5E), level 1..4,
// shape 0x49 + level: frame 0 at the button's corner and frame 1 170 pixels
// to its right. A button's rectangle is 321 pixels wide (x 159..479, centred
// on the screen) and as high as the shape, from y 120, 179, 238, 297. The
// selected level (DS:5BEC, 3 at start-up, kept between screens) is outlined
// in colour 0x44.
//
// Keys (13D0:29B3): up / '8' the level above (not before 1), down / '2' the
// one below (not after 4), Enter chooses the selected level, Escape cancels.
// Mouse (button base class, SEG1310): a press inside a button selects it and
// captures the mouse; releasing inside chooses it (13D0:2975).
//
// Choosing sets the difficulty (DS:7E19) and DS:2172 = (difficulty != 1),
// whose reader is not identified; the screen closes with the level, or 0.
//
// No Regret (13B8:27F9 .. 3080): the background is one frame, centred
// (13B8:2D79); every button is outlined, in colour 0xF1 when selected and
// 0xEE otherwise; a button selected (by a key or the mouse, 13B8:315F)
// stops the sounds and plays its own, 0x19D + level - 1. The main menu
// plays 0x1A1 when the screen closes (13B8:08A8).

#include "core/surface.hpp"
#include "engine/audio/audio.hpp"
#include "engine/world/render.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace crusader {

class DifficultyScreen {
public:
    static constexpr int kLevels = 4;
    static constexpr std::uint16_t kBackground = 0x49;
    static constexpr std::uint16_t kCursor = 0x26;
    static constexpr int kMouseX = 320, kMouseY = 400;
    static constexpr std::uint8_t kOutlineColour = 0x44;

    struct Rect {
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // inclusive
        bool contains(int x, int y) const { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }
    };

    static constexpr std::uint8_t kRegretSelectedColour = 0xF1, kRegretColour = 0xEE;
    static constexpr int kRegretFirstSound = 0x19D;
    static constexpr int kRegretCloseSound = 0x1A1;

    // `selected` is DS:5BEC (1..4). `audio` plays No Regret's button sounds.
    DifficultyScreen(ShapeCache& gumps, int& selected, Audio* audio = nullptr);

    void draw(Surface8& screen) const;
    // Gump key code (see gump_keys.hpp).
    void key(std::uint16_t code);
    void mouse_down(int x, int y);
    void mouse_move(int x, int y);
    void mouse_up(int x, int y);

    // Set once closed: the level chosen, or 0 when cancelled.
    std::optional<int> result() const { return result_; }
    int selected() const { return *selected_; }
    const Rect& button(int level) const { return buttons_[level - 1]; }

private:
    void draw_shape(Surface8& screen, std::uint16_t shape, int frame, int x, int y) const;
    void select(int level);  // 13B8:315F in No Regret

    Audio* audio_ = nullptr;
    ShapeCache* gumps_;
    int* selected_;
    std::array<Rect, kLevels> buttons_{};
    int captured_ = 0;  // level whose button holds the mouse, 0 for none
    std::optional<int> result_;
};

}  // namespace crusader
