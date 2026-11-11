#pragma once
// Minimal indexed-colour PNG writer (stored deflate blocks, no zlib needed).
// Used by tools and tests to inspect decoded graphics.

#include "core/palette.hpp"
#include "core/surface.hpp"

#include <filesystem>
#include <optional>

namespace crusader {

// Writes `surface` as an 8-bit indexed PNG. `transparent` marks one palette
// index as fully transparent (tRNS chunk).
void write_png(const std::filesystem::path& path, const Surface8& surface, const Palette& palette,
               std::optional<std::uint8_t> transparent = std::nullopt);
// Writes an RGB565 `surface` as an 8-bit RGB PNG.
void write_png(const std::filesystem::path& path, const Surface16& surface);

}  // namespace crusader

namespace crusader {

// Writes `surface` as an uncompressed colour-mapped TGA (type 1, 256 BGR
// entries, rows from the top), as No Regret's PIX pictures (TARGA.C, segment
// 12C8: 640 x 480, 8 bits). False when the file cannot be written.
bool write_tga(const std::filesystem::path& path, const Surface8& surface, const Palette& palette);

}  // namespace crusader
