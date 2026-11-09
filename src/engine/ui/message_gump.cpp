#include "engine/ui/message_gump.hpp"

#include <string_view>

namespace crusader {

namespace {

// 1300:004B: outline, fill, shade along the top and left, light along the
// bottom and right.
// @orig 1300:004B
void draw_raised(Surface8& s, const MessageGump::Rect& r, std::uint8_t c) {
    outline(s, r.x0, r.y0, r.x1, r.y1, 0);
    fill_rect(s, r.x0 + 1, r.y0 + 1, r.x1 - 1, r.y1 - 1, c);
    vline(s, r.x0 + 1, r.y0 + 1, r.y1 - 2, static_cast<std::uint8_t>(c - 2));
    hline(s, r.x0 + 2, r.y0 + 1, r.x1 - 2, static_cast<std::uint8_t>(c - 2));
    hline(s, r.x0 + 2, r.y1 - 1, r.x1 - 1, static_cast<std::uint8_t>(c + 2));
    vline(s, r.x1 - 1, r.y0 + 2, r.y1 - 2, static_cast<std::uint8_t>(c + 2));
}

// 1300:01A5: outline, fill, light along the top and left.
// @orig 1300:01A5
void draw_pressed(Surface8& s, const MessageGump::Rect& r, std::uint8_t c) {
    outline(s, r.x0, r.y0, r.x1, r.y1, 0);
    fill_rect(s, r.x0 + 1, r.y0 + 1, r.x1 - 1, r.y1 - 1, c);
    vline(s, r.x0 + 1, r.y0 + 1, r.y1 - 1, static_cast<std::uint8_t>(c + 2));
    hline(s, r.x0 + 2, r.y0 + 1, r.x1 - 1, static_cast<std::uint8_t>(c + 2));
}

}  // namespace

// @orig 1358:0046 1388:0000
MessageGump::MessageGump(const FontSet& fonts, std::string text, int screen_width, int screen_height)
    : fonts_(&fonts), text_(std::move(text)) {
    const TextWriter font(fonts, kFont);
    const TextWriter::Size ok = font.measure(kOk);
    button_ = {0, 0, ok.width + 8, ok.height + 4};
    Rect box{0, 0, button_.x1 + 9, button_.y1 + 13};
    const TextWriter::Size size = font.measure(text_);
    if (box.x1 - box.x0 + 1 < size.width + 8) box.x1 = size.width + 8;
    box.y1 += size.height;
    // The button centred across the box, 4 pixels above its bottom.
    const int dx = ((box.x1 - box.x0 + 1) >> 1) - ((button_.x1 - button_.x0 + 1) >> 1) - button_.x0;
    const int dy = (box.y1 - box.y0 + 1) - (button_.y1 - button_.y0 + 5) - button_.y0;
    button_ = {button_.x0 + dx, button_.y0 + dy, button_.x1 + dx, button_.y1 + dy};
    // The box centred on the screen.
    const int sx = (screen_width >> 1) - ((box.x1 - box.x0 + 1) >> 1) - box.x0;
    const int sy = (screen_height >> 1) - ((box.y1 - box.y0 + 1) >> 1) - box.y0;
    box_ = {box.x0 + sx, box.y0 + sy, box.x1 + sx, box.y1 + sy};
}

MessageGump::Rect MessageGump::button() const {
    return {box_.x0 + button_.x0, box_.y0 + button_.y0, box_.x0 + button_.x1, box_.y0 + button_.y1};
}

// @orig 1358:02DF 1388:00CC
void MessageGump::draw(Surface8& screen) const {
    draw_raised(screen, box_, kColour);
    TextWriter font(*fonts_, kFont);
    // 1358:02DF: line by line, each measured alone.
    const std::string_view text = text_;
    std::size_t start = 0;
    for (int line = 1;; ++line) {
        const std::size_t end = text.find('\n', start);
        const std::string_view part = text.substr(start, end == std::string_view::npos ? end : end - start);
        const TextWriter::Size size = font.measure(part);
        const int x = box_.x0 + ((box_.x1 - box_.x0 + 1) >> 1) - (size.width >> 1);
        font.print(screen, x, box_.y0 + size.height * line + 4, part);
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    // 1388:00CC: the button, drawn one pixel larger than its rectangle.
    const Rect b = button();
    const Rect frame{b.x0, b.y0, b.x1 + 1, b.y1 + 1};
    const auto colour = static_cast<std::uint8_t>(kColour + 2);
    if (pressed_) draw_pressed(screen, frame, colour);
    else draw_raised(screen, frame, colour);
    const int p = pressed_ ? 1 : 0;
    font.print(screen, b.x0 + p + 4, b.y0 + font.measure(kOk).height + p + 2, kOk);
}

void MessageGump::key(std::uint16_t code) {
    if (code != 0) closed_ = true;
}

void MessageGump::mouse_down(int x, int y) {
    if (closed_ || !button().contains(x, y)) return;
    captured_ = pressed_ = true;
}

void MessageGump::mouse_move(int x, int y) {
    if (captured_) pressed_ = button().contains(x, y);
}

void MessageGump::mouse_up(int x, int y) {
    if (!captured_) return;
    captured_ = pressed_ = false;
    if (button().contains(x, y)) closed_ = true;
}

}  // namespace crusader
