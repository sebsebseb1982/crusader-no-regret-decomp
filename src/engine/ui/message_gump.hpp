#pragma once
// The message box of the engine (GUMPSHAP_1358_0046, run by Gump_run_modal
// 1328:1588): a text over an "Ok" button (DS:4F5A), centred on the screen.
// R uses it ("DESIGNATION DE CIBLE ACTIVE." / "INACTIVE."), and so do the
// cheat keys and some errors.
//
// Font 0 (the default font, DS:4CE6). Colour c = DS:4D0E[5] = 0x14 for the
// box and c + 2 for the button (their frame colour, +0x41 and +0x37).
//
// Layout (1358:0046, rectangles inclusive, relative to the parent):
//   button   (0, 0, ok_width + 8, ok_height + 4)          (1388:0000)
//   box      (0, 0, button.x1 + 9, button.y1 + 13), x1 / y1 being the right
//            and bottom; x1 = text_width + 8 when the box is narrower than
//            that, then text_height higher;
//   the button centred across the box ((w + 1) >> 1 halves), its bottom 4
//   pixels above the box's; the box centred on the 640 x 480 screen.
//
// Drawing:
//   - the box (1300:038A with +0x46 = 0, then 1300:004B): outline colour 0,
//     filled with c, a shade c - 2 along the top and left inside it, a light
//     c + 2 along the bottom and right;
//   - each line i (1 based, 1358:02DF) centred across the box, the pen at y
//     = top + h * i + 4 where h is the height of that line alone;
//   - the button (1388:00CC), one pixel wider and higher than its rectangle:
//     raised like the box, or pressed (1300:01A5: outline 0, filled, a light
//     c + 2 along the top and left), its text at (x + p + 4, y + h + p + 2)
//     where p is 1 when pressed.
//
// Any key closes it (vtable +0x18, 1358:02C8: the gump's close, +0x3C). The
// mouse: a press in the button presses it; moving out and back in releases
// and presses it again; releasing in it closes the box (button base class,
// segment 1310). While it is open the game is frozen (Gump_run_modal only
// draws and routes events).

#include "core/font.hpp"
#include "core/surface.hpp"

#include <string>

namespace crusader {

class MessageGump {
public:
    static constexpr int kFont = 0;
    static constexpr std::uint8_t kColour = 0x14;  // DS:4D0E + 5
    static constexpr const char* kOk = "Ok";       // DS:4F5A

    struct Rect {
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // inclusive
        bool contains(int x, int y) const { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }
    };

    MessageGump(const FontSet& fonts, std::string text, int screen_width = 640, int screen_height = 480);

    void draw(Surface8& screen) const;
    // Gump key code (see gump_keys.hpp).
    void key(std::uint16_t code);
    void mouse_down(int x, int y);
    void mouse_move(int x, int y);
    void mouse_up(int x, int y);

    bool closed() const { return closed_; }
    const std::string& text() const { return text_; }
    const Rect& box() const { return box_; }
    // The button on screen (its rectangle; drawn one pixel larger).
    Rect button() const;

private:
    const FontSet* fonts_;
    std::string text_;
    Rect box_;     // on screen
    Rect button_;  // relative to the box
    bool captured_ = false;
    bool pressed_ = false;
    bool closed_ = false;
};

}  // namespace crusader
