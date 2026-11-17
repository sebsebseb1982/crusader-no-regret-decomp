#pragma once
// Language files (port addition): the texts the port prints itself (its own
// screens, and the texts the original keeps in its executable: item names,
// messages, Weasel's shop...) in another language than the game data's. The
// data's own texts (FLX files, movie subtitles, pictures) are not concerned:
// a translation changes those files.
//
// One file per language in the "languages" folder, UTF-8 text:
//   - lines starting with '#' are comments, blank lines are skipped;
//   - "@name = <name>": the language's name, shown by the enhancements window
//     (the file's name when missing);
//   - "<English text> = <translation>": the English text of the port (see
//     languages/template.txt for the list), then its translation. "\n" in
//     either is a line break, "\\" a backslash. An empty translation keeps
//     the original text.
// Spaces around the texts are dropped. The translations are converted to
// code page 850, the fonts' (characters out of it become '?'), and must keep
// the "%" conversions of their English text, in order (else they are dropped,
// with a warning).

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace crusader {

// UTF-8 to code page 850 ('?' for what it lacks, or for bad UTF-8), and back.
std::string utf8_to_cp850(std::string_view utf8);
std::string cp850_to_utf8(std::string_view cp850);

class Translation {
public:
    // `warnings` (optional) gets one line per line ignored.
    static Translation parse(std::string_view utf8, std::vector<std::string>* warnings = nullptr);
    // Throws std::runtime_error when the file cannot be read.
    static Translation load(const std::filesystem::path& path, std::vector<std::string>* warnings = nullptr);

    // The name of the language (code page 850), empty when the file has none.
    const std::string& name() const { return name_; }
    // The translation of `english`, null when there is none.
    const char* find(std::string_view english) const;
    std::size_t size() const { return texts_.size(); }

private:
    struct Hash {
        using is_transparent = void;
        std::size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
    };
    std::string name_;
    std::unordered_map<std::string, std::string, Hash, std::equal_to<>> texts_;
};

}  // namespace crusader
