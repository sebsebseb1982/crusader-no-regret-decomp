#include "core/movie_subtitles.hpp"

#include <cstdlib>
#include <fstream>
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
// @orig 1470:0398
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

// @orig 1470:0239
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

// @orig 1470:00FB
std::vector<MovieSubtitle> load_movie_subtitles(const std::filesystem::path& path, const FontSet& fonts, int font) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream text;
    text << in.rdbuf();
    return parse_movie_subtitles(text.str(), fonts, font);
}

}  // namespace crusader
