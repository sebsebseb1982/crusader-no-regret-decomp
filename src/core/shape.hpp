#pragma once
// Crusader shape (SHAPES.FLX, GUMPS.FLX, MOUSE.SHP, ...): a set of RLE frames.
//
// Shape:   u16 ?, u16 ?, u16 frame_count, then frame_count * 8-byte entries
//          { u24 offset, u8 ?, u32 length }
// Frame:   at offset: [8 bytes ?, world shapes only] i32 compressed, i32 width,
//          i32 height, i32 xoff, i32 yoff (anchor inside the frame), then
//          height * u32 line offsets (each relative to its own position), then
//          RLE data. Gumps (GUMPS.FLX) and the cursor (MOUSE.SHP) omit the
//          8-byte prefix.
// Line:    repeat { u8 skip (transparent run), u8 n; if compressed: n&1 = fill
//          run of one byte, n>>1 = length; else n raw bytes } until width.

#include "core/surface.hpp"
#include "core/xform.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace crusader {

enum class ShapeLayout {
    World,   // SHAPES.FLX: 28-byte frame header
    Flat2D,  // GUMPS.FLX, MOUSE.SHP: 20-byte frame header
};

struct ShapeFrame {
    int width = 0;
    int height = 0;
    int xoff = 0;  // anchor position inside the frame
    int yoff = 0;
    bool compressed = false;
    std::span<const std::uint8_t> rle;   // RLE stream, line offsets index into it
    std::vector<std::uint32_t> lines;

    // Draws with the anchor at (x, y); transparent runs are skipped. With
    // `xform`, pixels go through the translucency tables (asm_blit_4).
    void draw(Surface8& dst, int x, int y, bool mirrored = false, const XformTable* xform = nullptr) const;

    // Colour of the frame pixel at (fx, fy) in frame coordinates, -1 if transparent.
    int pixel(int fx, int fy) const;

    // True when every line's RLE runs add up exactly to `width`: a frame parsed
    // with the wrong layout decodes without overflow but fails this check.
    bool runs_match_width() const;
};

// Picks the frame layout for an archive by file name (GUMPS.FLX, MOUSE.SHP -> Flat2D).
ShapeLayout layout_for_archive(const std::string& filename);

class Shape {
public:
    // Throws std::out_of_range / std::runtime_error on malformed data. With
    // `skip_bad_frames`, a malformed frame becomes an empty one (0 x 0) and is
    // counted in bad_frames() instead.
    static Shape parse(std::span<const std::uint8_t> data, ShapeLayout layout = ShapeLayout::World,
                       bool skip_bad_frames = false);

    std::size_t bad_frames() const { return bad_frames_; }

    const std::vector<ShapeFrame>& frames() const { return frames_; }

private:
    std::vector<ShapeFrame> frames_;
    std::size_t bad_frames_ = 0;
};

}  // namespace crusader
