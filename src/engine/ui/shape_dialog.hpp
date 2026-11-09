#pragma once
// The dialogs of the game view drawn from GUMPS.FLX shapes (WORLD.C, segment
// 13F8): a question with "yes" and "no" buttons (13F8:0000) and a notice with
// one button (13F8:0383). Both are window gumps that draw only their shape
// (frame 0, opaque; 13F8:0349 / 058C) at the top-left corner of their
// rectangle, the shape's (0, 0, w - 1, h - 1) moved to ((640 - w) >> 1,
// (479 - h) >> 1). The buttons are shape buttons (shape_button below), their
// top-left corner given relative to the dialog.
//
// Question (World_on_key): F4 "load the quick save?" shape 0x11, F5 "quick
// save?" 0x12, Alt+X "quit?" 0x15; "yes" is shape 0x13, button id 1, and
// "no" shape 0x14, id 0, at y 47 and x 57 / 125 for Alt+X, 49 / 121 for F4
// and F5 (55 / 135 for the three in the US executable). Keys (1368:0F2A,
// the others do not reach it): o O j J y Y n N s S and Escape; 13F8:0299 closes with 1
// for Y, J or O (after toupper: "yes", "ja", "oui"), with 0 for the others.
// A button activated closes with its id (13F8:0237).
//
// Notice: after the quick load, shape 0x4E, after the quick save 0x4F; the
// button "Ok" is shape 0x50 at (91, 47). Enter and Escape close it, as the
// button does.
//
// The cursor: frame 0x24, shown (cursor_reset / cursor_show at the end of
// both constructors); the caller handles it.

#include "engine/ui/modal_gump.hpp"
#include "engine/world/render.hpp"

#include <cstdint>
#include <vector>

namespace crusader {

// A shape button (1378:0000): frame 0, or 1 while selected. A point is in it
// when it is in its rectangle and on an opaque pixel of the frame drawn
// (1378:0105). The mouse (button base class, segment 1310): a press in it
// captures the mouse and selects it; moving out deselects it and back in
// selects it again; releasing in it activates it, and it stays selected.
struct ShapeButton {
    struct Rect {
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // inclusive
        bool contains(int x, int y) const { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }
    };

    ShapeButton(ShapeCache& gumps, std::uint16_t shape, int id, int x, int y);

    bool contains(int x, int y) const;
    void draw(Surface8& screen) const;

    ShapeCache* gumps;
    std::uint16_t shape;
    int id;
    Rect rect;  // on screen
    bool selected = false;
};

// The mouse over a set of shape buttons: the id of the button activated by
// a release, or -1.
class ShapeButtons {
public:
    std::vector<ShapeButton> buttons;

    void mouse_down(int x, int y);
    void mouse_move(int x, int y);
    int mouse_up(int x, int y);
    void draw(Surface8& screen) const;

private:
    int captured_ = -1;
};

class ShapeDialog : public ModalGump {
public:
    static constexpr std::uint16_t kQuickLoadQuestion = 0x11;
    static constexpr std::uint16_t kQuickSaveQuestion = 0x12;
    static constexpr std::uint16_t kQuitQuestion = 0x15;
    static constexpr std::uint16_t kQuickLoadNotice = 0x4E;
    static constexpr std::uint16_t kQuickSaveNotice = 0x4F;

    // 13F8:0000 with the "yes" (0x13) and "no" (0x14) buttons.
    static ShapeDialog question(ShapeCache& gumps, std::uint16_t shape);
    // 13F8:0383 with the "Ok" button (0x50).
    static ShapeDialog notice(ShapeCache& gumps, std::uint16_t shape);

    void draw(Surface8& screen) const override;
    void key(std::uint16_t code) override;
    void mouse_down(int x, int y) override { buttons_.mouse_down(x, y); }
    void mouse_move(int x, int y) override { buttons_.mouse_move(x, y); }
    void mouse_up(int x, int y) override;

    bool closed() const override { return closed_; }
    int result() const override { return result_; }
    bool over_view() const override { return true; }

    std::uint16_t shape() const { return shape_; }
    const ShapeButton::Rect& rect() const { return rect_; }
    const std::vector<ShapeButton>& buttons() const { return buttons_.buttons; }

private:
    ShapeDialog(ShapeCache& gumps, std::uint16_t shape, bool question);
    void close(int result);

    ShapeCache* gumps_;
    std::uint16_t shape_;
    bool question_;
    ShapeButton::Rect rect_;
    ShapeButtons buttons_;
    bool closed_ = false;
    int result_ = 0;
};

}  // namespace crusader
