#pragma once
// The software mouse cursor (MOUSECTL.C, segment 1360) and the game's cursor
// object built on it (U9MOUSE.C, segment 13D8), with STATIC/MOUSE.SHP.
//
// MouseCtl keeps a stack of 8 cursor frames (+0x186, level +0x196): push,
// pop (never below level 0) and reset (back to level 0) set the frame of the
// current level. The cursor is drawn with its frame anchor on the mouse
// position, only while shown (+0x17E); the original saves and restores the
// screen under it, the port draws it last over each frame.
//
// U9MOUSE (inherited from Ultima 8) turns stack entries 0 and 0x19 into a
// directional cursor: direction 0..7 of the mouse around the point (320, 200)
// (+0x19C, never changed afterwards), plus the entry, plus 8 when the entry
// is 0 and the mouse is outside a 61 x 51 box around that point ("run").
// Other entries are frames as such. The result is clamped to the last frame.
// Crusader only shows the cursor in menus (frames 0x24 and 0x25); in play it
// stays hidden, and MOUSE.SHP frames 0..7 are eight copies of one picture.
//
// The mouse driver range is x 0..639 and y 0..479, or 0..439 while the status
// bar is shown (StatusBar_show / hide), which also put the mouse at (320, 240).

#include "core/shape.hpp"
#include "core/surface.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace crusader {

class MouseCursor {
public:
    static constexpr int kStackSize = 8;
    static constexpr std::uint16_t kDirectional = 0;      // walk / run arrows
    static constexpr std::uint16_t kDirectionalAlt = 0x19;
    static constexpr std::uint16_t kBusy = 0x23;
    static constexpr std::uint16_t kArrow = 0x24;
    static constexpr std::uint16_t kMenuArrow = 0x25;
    static constexpr int kCentreX = 320, kCentreY = 200;

    static MouseCursor load(const std::filesystem::path& mouse_shp);

    // cursor_set_frame / cursor_push / cursor_reset / cursor_pop (1360:0B92..0D06).
    void set_frame(std::uint16_t frame) { stack_[level_] = frame; }
    void push(std::uint16_t frame);
    void reset(std::uint16_t frame);
    std::uint16_t pop();  // returns the frame of the level left
    std::uint16_t stack_frame() const { return stack_[level_]; }
    int level() const { return level_; }

    void show() { visible_ = true; }
    void hide() { visible_ = false; }
    bool visible() const { return visible_; }

    // Mouse position, clamped to the driver range.
    void move_to(int x, int y);
    int x() const { return x_; }
    int y() const { return y_; }
    void set_bottom(int max_y);  // 479 or 439
    int bottom() const { return max_y_; }

    // game_mouse_cursor_frame (13D8:01A0).
    int frame() const;
    // game_mouse_zone: 1 inside the box around (320, 200), 2 outside.
    static int zone(int x, int y);
    // Direction 0..7 of (x, y) seen from (320, 200), 0 = up, clockwise.
    static int direction(int x, int y);

    void draw(Surface8& screen) const;
    std::size_t frame_count() const { return shape_.frames().size(); }

private:
    std::vector<std::uint8_t> data_;  // the frames point into it
    Shape shape_;
    std::array<std::uint16_t, kStackSize> stack_{};
    int level_ = 0;
    bool visible_ = false;
    int x_ = 0, y_ = 0;
    int max_y_ = 479;
};

}  // namespace crusader
