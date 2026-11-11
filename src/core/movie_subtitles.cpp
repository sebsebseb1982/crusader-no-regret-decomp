#include "core/movie_subtitles.hpp"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>

namespace crusader {

namespace {

// The C library's ctype table (DS:6B0D), bits 4 | 8: ASCII letters only.
bool is_letter(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

// FUN_1470_04f8: the next '@' after `from` (searching from from + 1), or npos.
std::size_t next_entry(std::string_view s, std::size_t from) {
    const std::size_t at = s.find('@', from + 1);
    return at == std::string_view::npos ? std::string_view::npos : at;
}

// FUN_1470_05cc: the frame number of the "@frame <n>" line at `at`.
std::int32_t frame_number(std::string_view s, std::size_t at) {
    if (at + 7 > s.size()) return 0;
    return static_cast<std::int32_t>(std::atol(std::string(s.substr(at + 7, 12)).c_str()));
}

// FUN_1470_0398: the line after the one at `at`, if it is a subtitle.
// @orig ?: No Remorse 1470:0398, the No Remorse video player only (No Regret: xan_player.hpp)
std::optional<std::string> text_after(std::string_view s, std::size_t at) {
    const std::size_t cr = s.find('\r', at);
    if (cr == std::string_view::npos) return std::nullopt;
    const std::size_t start = cr + 2;
    if (start >= s.size()) return std::nullopt;
    const char first = s[start];
    if (first == '@' || (!is_letter(first) && first != '.')) return std::nullopt;
    const std::size_t end = s.find('\r', start);
    return std::string(s.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
}

}  // namespace

// @orig ?: No Remorse 1470:0239, the No Remorse video player only (No Regret: xan_player.hpp)
std::vector<MovieSubtitle> parse_movie_subtitles(std::string_view s, const FontSet& fonts, int font) {
    std::vector<MovieSubtitle> out;
    const TextWriter writer(fonts, font);
    std::size_t at = 0;
    while (at != std::string_view::npos && at < s.size()) {
        MovieSubtitle e;
        e.text = text_after(s, at);
        const std::size_t next = next_entry(s, at);
        e.delay = (next == std::string_view::npos ? 0 : frame_number(s, next)) - frame_number(s, at);
        if (e.text) {
            for (std::size_t i = 0; i + 1 < e.text->size(); ++i) {
                e.width += writer.char_width(static_cast<unsigned char>((*e.text)[i]));
            }
        }
        out.push_back(std::move(e));
        at = next;
    }
    return out;
}

// @orig ?: No Remorse 1470:00FB, the No Remorse video player only (No Regret: xan_player.hpp)
std::vector<MovieSubtitle> load_movie_subtitles(const std::filesystem::path& path, const FontSet& fonts, int font) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream text;
    text << in.rdbuf();
    return parse_movie_subtitles(text.str(), fonts, font);
}

// @orig 1448:008B 1440:0732
std::vector<RegretSubtitle> parse_regret_movie_subtitles(std::span<const std::uint8_t> file) {
    std::vector<RegretSubtitle> out;
    std::size_t p = 0;
    for (; p + 4 <= file.size(); ++p) {
        if (file[p] == 'E' && file[p + 1] == 'N' && file[p + 2] == 'G' && file[p + 3] == 'L') break;
    }
    if (p + 4 > file.size()) return out;
    p += 8;
    while (p + 5 <= file.size()) {
        const auto frame = static_cast<std::int32_t>(file[p] | file[p + 1] << 8 | file[p + 2] << 16 |
                                                     static_cast<std::uint32_t>(file[p + 3]) << 24);
        if (frame < 0) break;
        const std::size_t length = file[p + 4];
        const std::size_t text = p + 5;
        if (text + length > file.size()) break;
        std::size_t end = text;
        while (end < text + length && file[end] != 0) ++end;
        out.push_back({frame, std::string(file.begin() + static_cast<std::ptrdiff_t>(text),
                                          file.begin() + static_cast<std::ptrdiff_t>(end))});
        p = text + length;
    }
    return out;
}

// @orig 1448:0B5A
std::vector<RegretSubtitle> load_regret_movie_subtitles(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return parse_regret_movie_subtitles(bytes);
}

}  // namespace crusader
