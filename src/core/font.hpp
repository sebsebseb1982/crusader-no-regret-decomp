#pragma once
// Fonts (CAFONT.C, segment 1318) and the text stream they plug into (HUGE.C,
// segment 12C0).
//
// STATIC/FONTS.FLX holds one shape per font (2D frame layout, like GUMPS.FLX);
// frame n is the glyph of character code n, taken unsigned. The font handler
// (CAFONT_1318_0215, a 32-entry shape cache) is created by CRUSYS.C.
//
// A font object (CAFONT_1318_0148, 12 bytes) is a text stream {vtable, x, y}
// plus the font number and two spacing values copied from tables in the
// executable (DS:4918 and DS:4958): a glyph's advance is its frame width plus
// the first, a line's height its frame height plus the second. Font numbers
// above 31 become 0 (CAFONT_1318_028c).
//
// Writing a character (HUGE_12c0_0071) draws its glyph with the frame anchor at
// the stream position, then: '\n' moves down by the glyph height and back to
// the left edge of the current viewport; '\r' only goes back to that edge; any
// other character advances x by its width. '\n' and '\r' have glyphs of their
// own, drawn like any other.

#include "core/flex.hpp"
#include "core/shape.hpp"
#include "core/surface.hpp"

#include <filesystem>
#include <string_view>
#include <vector>

namespace crusader {

class FontSet {
public:
    static constexpr int kMaxFonts = 32;

    static FontSet load(const std::filesystem::path& fonts_flx);

    // Glyph of character `c`, or nullptr when the font or the frame does not
    // exist (the original does not check: it would read past the frame table).
    const ShapeFrame* glyph(int font, unsigned char c) const;

    // Spacing tables DS:4918 (added to the advance) and DS:4958 (to the height).
    static int extra_width(int font);
    static int extra_height(int font);

    // Frames that could not be read (see load()).
    std::size_t bad_frames() const { return bad_frames_; }

private:
    FlexArchive flex_;          // the frames point into it
    std::vector<Shape> fonts_;  // index = font number, empty shape when absent
    std::size_t bad_frames_ = 0;
};

class TextWriter {
public:
    TextWriter(const FontSet& fonts, int font);

    int font() const { return font_; }
    int x() const { return x_; }
    int y() const { return y_; }
    void move_to(int x, int y) { x_ = x; y_ = y; }

    // Advance and line height of `c` (CAFONT_1318_00f5).
    int char_width(unsigned char c) const;
    int char_height(unsigned char c) const;

    // Anchor height of glyph 0, i.e. the distance from the top of a line to the
    // pen position (CAFONT_1318_0000).
    int baseline() const;

    // HUGE_12c0_0071. `left` is the left edge of the target viewport.
    void put(Surface8& dst, unsigned char c, int left = 0);

    // HUGE_12c0_02d7 (printf at a position; formatting is done by the caller).
    void print(Surface8& dst, int x, int y, std::string_view text, int left = 0);

    // HUGE_12c0_0344 / 0183: writes characters while the next one fits within
    // `max_width` of the starting x; returns how many were written.
    int print_clipped(Surface8& dst, int x, int y, int max_width, std::string_view text, int left = 0);

    struct Size {
        int width = 0;
        int height = 0;
    };
    // HUGE_12c0_01f9. Width: the widest line, where a '\n' ends the line and its
    // own advance counts in the next one. Height: the height of each '\n' plus
    // the height of the last character.
    Size measure(std::string_view text) const;

private:
    const FontSet* fonts_;
    int font_;
    int x_ = 0;
    int y_ = 0;
};

}  // namespace crusader
