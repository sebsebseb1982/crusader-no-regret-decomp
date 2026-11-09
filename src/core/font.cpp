#include "core/font.hpp"

#include <algorithm>
#include <array>

namespace crusader {

namespace {

// DS:4918 and DS:4958 in CRUSADER.EXE (French, 12/10/1995), 32 words each.
constexpr std::array<int, FontSet::kMaxFonts> kExtraWidth = {
    1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
constexpr std::array<int, FontSet::kMaxFonts> kExtraHeight = {
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

}  // namespace

// @orig 1318:0215
FontSet FontSet::load(const std::filesystem::path& fonts_flx) {
    FontSet set;
    set.flex_ = FlexArchive::open(fonts_flx);
    const std::size_t count = std::min<std::size_t>(set.flex_.count(), kMaxFonts);
    set.fonts_.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto data = set.flex_.entry(i);
        if (!data.empty()) {
            // Font 2 announces 256 frames but has 128: entries 128..255 point
            // into its glyph data and are dropped (codes >= 128 draw nothing).
            set.fonts_[i] = Shape::parse(data, ShapeLayout::Flat2D, true);
            set.bad_frames_ += set.fonts_[i].bad_frames();
        }
    }
    return set;
}

const ShapeFrame* FontSet::glyph(int font, unsigned char c) const {
    if (font < 0 || font >= static_cast<int>(fonts_.size())) return nullptr;
    const auto& frames = fonts_[font].frames();
    return c < frames.size() ? &frames[c] : nullptr;
}

int FontSet::extra_width(int font) { return font >= 0 && font < kMaxFonts ? kExtraWidth[font] : 0; }
int FontSet::extra_height(int font) { return font >= 0 && font < kMaxFonts ? kExtraHeight[font] : 0; }

TextWriter::TextWriter(const FontSet& fonts, int font)
    : fonts_(&fonts), font_(font >= 0 && font < FontSet::kMaxFonts ? font : 0) {}

int TextWriter::char_width(unsigned char c) const {
    const ShapeFrame* g = fonts_->glyph(font_, c);
    return (g ? g->width : 0) + FontSet::extra_width(font_);
}

int TextWriter::char_height(unsigned char c) const {
    const ShapeFrame* g = fonts_->glyph(font_, c);
    return (g ? g->height : 0) + FontSet::extra_height(font_);
}

int TextWriter::baseline() const {
    const ShapeFrame* g = fonts_->glyph(font_, 0);
    return g ? g->yoff : 0;
}

// @orig 12C0:0071 1318:002E
void TextWriter::put(Surface8& dst, unsigned char c, int left) {
    if (const ShapeFrame* g = fonts_->glyph(font_, c)) {
        g->draw(dst, x_, y_);
    }
    if (c == '\n') {
        y_ += char_height(c);
        x_ = left;
    } else if (c == '\r') {
        x_ = left;
    } else {
        x_ += char_width(c);
    }
}

// @orig 12C0:015C
void TextWriter::print(Surface8& dst, int x, int y, std::string_view text, int left) {
    move_to(x, y);
    for (char c : text) {
        put(dst, static_cast<unsigned char>(c), left);
    }
}

// @orig 12C0:0183
int TextWriter::print_clipped(Surface8& dst, int x, int y, int max_width, std::string_view text, int left) {
    move_to(x, y);
    int written = 0;
    for (char ch : text) {
        const auto c = static_cast<unsigned char>(ch);
        if (x_ + char_width(c) > x + max_width) break;
        put(dst, c, left);
        ++written;
    }
    return written;
}

// @orig 12C0:01F9
TextWriter::Size TextWriter::measure(std::string_view text) const {
    int line = 0;
    int widest = 0;
    int height = 0;
    int last_height = 0;
    for (char ch : text) {
        const auto c = static_cast<unsigned char>(ch);
        last_height = char_height(c);
        if (c == '\n') {
            height += last_height;
            widest = std::max(widest, line);
            line = 0;
        }
        line += char_width(c);
    }
    height += last_height;
    return {std::max(widest, line), height};
}

}  // namespace crusader
