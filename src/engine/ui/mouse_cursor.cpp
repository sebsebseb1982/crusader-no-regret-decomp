#include "engine/ui/mouse_cursor.hpp"

#include "core/flex.hpp"

#include <algorithm>

namespace crusader {

// @orig 13D8:0000 1360:0D5A
MouseCursor MouseCursor::load(const std::filesystem::path& mouse_shp) {
    MouseCursor c;
    c.data_ = read_file(mouse_shp);
    c.shape_ = Shape::parse(c.data_, ShapeLayout::Flat2D);
    return c;
}

void MouseCursor::push(std::uint16_t frame) {
    if (++level_ == kStackSize) level_ = kStackSize - 1;
    set_frame(frame);
}

void MouseCursor::reset(std::uint16_t frame) {
    level_ = 0;
    set_frame(frame);
}

// @orig 1360:0D06
std::uint16_t MouseCursor::pop() {
    const std::uint16_t left = stack_[level_];
    if (--level_ < 0) level_ = 0;
    set_frame(stack_[level_]);
    return left;
}

void MouseCursor::move_to(int x, int y) {
    x_ = std::clamp(x, 0, 639);
    y_ = std::clamp(y, 0, max_y_);
}

void MouseCursor::set_bottom(int max_y) {
    max_y_ = max_y;
    move_to(x_, y_);
}

// @orig 13D8:008D
int MouseCursor::zone(int x, int y) {
    // First the screen less a 20-pixel border, then the box around the centre;
    // both rectangles inclusive (rect_contains_point_2).
    const bool on_screen = x >= 20 && x <= 620 && y >= 20 && y <= 460;
    const bool near = x >= kCentreX - 30 && x <= kCentreX + 30 && y >= kCentreY - 25 && y <= kCentreY + 25;
    return on_screen && near ? 1 : 2;
}

// @orig 13D8:031A
int MouseCursor::direction(int x, int y) {
    const int dx = x - kCentreX;
    const int dy = y - kCentreY;
    if (dx == 0) return dy < 0 ? 0 : 4;
    const int r = dy * 8 / dx;  // slope in eighths, truncated toward zero
    if (r < -2 && r > -12) return dx < 0 ? 5 : 1;
    if (r < -11 || r > 11) return dy < 1 ? 0 : 4;
    if (r < 12 && r > 2) return dy < 1 ? 7 : 3;
    return dx < 0 ? 6 : 2;  // -3 < r < 3
}

// @orig 13D8:01A0
int MouseCursor::frame() const {
    const std::uint16_t entry = stack_[level_];
    int f = entry;
    if (entry == kDirectional || entry == kDirectionalAlt) {
        f = direction(x_, y_) + static_cast<std::int8_t>(entry & 0xFF);
        if (entry == kDirectional) f += (zone(x_, y_) - 1) * 8;
    }
    const int count = static_cast<int>(frame_count());
    return f < count ? f : count - 1;
}

void MouseCursor::draw(Surface8& screen) const {
    if (!visible_ || shape_.frames().empty()) return;
    shape_.frames()[frame()].draw(screen, x_, y_);
}

}  // namespace crusader
