#include "engine/ui/computer_gump.hpp"

#include "engine/ui/gump_keys.hpp"

namespace crusader {

namespace {

const ShapeFrame* gump_frame(ShapeCache& gumps, std::uint16_t frame) {
    const Shape* s = gumps.get(ComputerGump::kShape);
    return s && frame < s->frames().size() ? &s->frames()[frame] : nullptr;
}

}  // namespace

// @orig 13A0:0034
ComputerGump::ComputerGump(ShapeCache& gumps, const FontSet& fonts, const std::string& text,
                           std::function<void(int)> play_sfx, int screen_width, int screen_height)
    : gumps_(&gumps), fonts_(&fonts), play_sfx_(std::move(play_sfx)), text_(wrap(text)) {
    // GumpShape_rect of frame 0, made taller by frame 1's height, centred.
    const ShapeFrame* top = gump_frame(gumps, 0);
    const ShapeFrame* bottom = gump_frame(gumps, 1);
    const int width = top ? top->width : 0;
    const int height = (top ? top->height : 0) + (bottom ? bottom->height : 0);
    x_ = (screen_width - width) >> 1;
    y_ = (screen_height - height) >> 1;
}

// @orig 13A0:03C8
std::string ComputerGump::wrap(const std::string& text) {
    std::string s = text;
    int space = -1;
    int count = 0;
    for (int i = 0; i < static_cast<int>(s.size()) && s[static_cast<std::size_t>(i)] != 0; ++i) {
        const char c = s[static_cast<std::size_t>(i)];
        if (c == ' ') space = i;
        if (c == '^') {
            count = 0;
            space = -1;
        }
        if (count >= kColumns && space >= 0) {
            s[static_cast<std::size_t>(space)] = '^';
            count = i - space;
            space = -1;
        }
        if (c != 1) ++count;
    }
    return s;
}

// @orig 13A0:0487
void ComputerGump::layout() {
    column_ = 0;
    row_ = 0;
    for (int i = page_start_; i < shown_; ++i) {
        const char c = at(i);
        if (c == '^') {
            column_ = 0;
            ++row_;
        } else if (c != 1 && c != '*') {
            ++column_;
        }
        if (column_ >= kColumns) {
            column_ = 0;
            ++row_;
        }
        if (row_ == kRows) page_full_ = true;
    }
    cursor_ = false;
    if (page_full_ || at(shown_) == 0) return;
    ++shown_;
    cursor_ = !skip_;
}

// @orig 13A0:08D2
void ComputerGump::run() {
    if (ended()) {
        layout();  // a redraw: the last character in place of the cursor
        return;
    }
    if (skip_) {
        while (!page_full_ && !ended()) layout();
        skip_ = false;
    }
    // The redraw (13A0:02D1) lays the page out again, one character more.
    layout();
    if (page_full_) {
        skip_ = false;
        return;
    }
    const char c = at(shown_ - 1);
    if (c == '*') {
        next_run_ += kStarPause;
        return;
    }
    if (c != '\n' && play_sfx_) play_sfx_(kSound);
    next_run_ += 1;
}

// Gump_run_modal: the gump's run at each pass, here once its wait is over.
// @orig 1328:1588
void ComputerGump::update(std::uint32_t ticks) {
    if (closed_ || ticks < next_run_) return;
    next_run_ = ticks;
    run();
}

// @orig 13A0:02D1 13A0:0487
void ComputerGump::draw(Surface8& screen) const {
    if (const ShapeFrame* f = gump_frame(*gumps_, 0)) f->draw(screen, x_, y_);
    if (const ShapeFrame* f = gump_frame(*gumps_, 1)) f->draw(screen, x_, y_ + kBottomY);
    TextWriter writer(*fonts_, kFont);
    const auto cell = [this, &writer](int column, int row) {
        writer.move_to(x_ + kTextX + column * kCharWidth, y_ + kTextY + row * kRowHeight);
    };
    // The character under the cursor is drawn by the next redraw.
    const int end = cursor_ ? shown_ - 1 : shown_;
    int column = 0;
    int row = 0;
    for (int i = page_start_; i < end; ++i) {
        const char c = at(i);
        if (c == '^') {
            column = 0;
            ++row;
        } else if (c != 1 && c != '*') {
            cell(column, row);
            writer.put(screen, static_cast<unsigned char>(c));
            ++column;
        }
        if (column >= kColumns) {
            column = 0;
            ++row;
        }
    }
    if (cursor_) {
        cell(column_, row_);
        writer.put(screen, kCursor);
    }
    if (page_full_) writer.print(screen, x_ + kTextX, y_ + kTextY + row_ * kRowHeight, more());
}

// @orig 13A0:0A2A
void ComputerGump::key(std::uint16_t code) {
    if (page_full_) {
        page_full_ = false;
        column_ = 0;
        row_ = 0;
        page_start_ = shown_;
        return;
    }
    if (code != gump_key::kEscape && code != gump_key::kEnter) return;
    if (ended()) closed_ = true;
    else if (code == gump_key::kEscape) skip_ = true;
}

// @orig 13A0:0887
void ComputerGump::mouse_down(int, int) {
    if (ended()) closed_ = true;
    else skip_ = true;
}

}  // namespace crusader
