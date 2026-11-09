#pragma once
// Subtitles of the cutscenes, FLICS/<movie>.TXT (segment 1470, read by the
// video player when "subtitles=true" is in CRUSADER.CFG, DS:0179).
//
// The file is lines ending in "\r\n": "@frame <n>" lines, each followed by
// the line of text shown from frame n on (or by another "@frame" line: no
// text). FUN_1470_0239 turns it into one entry per "@frame" line:
//   - delay: the next "@frame" number minus this one (FUN_1470_05cc reads the
//     number 7 characters after the '@'); for the last entry 0 minus its
//     number, a delay that never runs out;
//   - text (FUN_1470_0398): the line after the "@frame" line, unless it
//     starts with '@', or with neither a letter nor '.';
//   - width: the advances of the text's characters but the last (the font's
//     vtable +0x18, Font_char_width), used to centre it.
// The bytes are kept as they are (code page 850, as the fonts).

#include "core/font.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace crusader {

struct MovieSubtitle {
    std::int32_t delay = 0;  // frames until the next entry
    std::optional<std::string> text;
    int width = 0;
};

// The entries of `text`, widths measured with `font` of `fonts`.
std::vector<MovieSubtitle> parse_movie_subtitles(std::string_view text, const FontSet& fonts, int font);

// Reads the file; an empty list when it cannot be read.
std::vector<MovieSubtitle> load_movie_subtitles(const std::filesystem::path& path, const FontSet& fonts, int font);

}  // namespace crusader
