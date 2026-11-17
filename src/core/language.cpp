#include "core/language.hpp"

#include "core/translation.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <memory>
#include <system_error>

namespace crusader {

namespace {

Language g_language = Language::French;
bool g_regret = false;

std::filesystem::path g_languages_dir;
LanguageChoice g_choice{"", "AUTO"};
// The files read, by id (null: none or unreadable). Every translation read is
// kept in g_read so that the texts handed out stay valid.
std::map<std::string, const Translation*, std::less<>> g_loaded;
std::vector<std::unique_ptr<Translation>> g_read;

// @orig none: language files of the port
std::string lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// The language files, by id.
// @orig none: language files of the port
std::vector<std::pair<std::string, std::filesystem::path>> language_files() {
    std::vector<std::pair<std::string, std::filesystem::path>> files;
    std::error_code error;
    if (g_languages_dir.empty() || !std::filesystem::is_directory(g_languages_dir, error)) return files;
    for (const auto& entry : std::filesystem::directory_iterator(g_languages_dir, error)) {
        const auto& path = entry.path();
        if (!entry.is_regular_file(error) || lower(path.extension().string()) != ".txt") continue;
        files.emplace_back(lower(path.stem().string()), path);
    }
    std::sort(files.begin(), files.end());
    return files;
}

// @orig none: language files of the port
std::string name_of(const std::string& id, const Translation& t) {
    if (!t.name().empty()) return t.name();
    std::string name = id;
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return name;
}

// The file of `id`, read on first use; null when there is none.
// @orig none: language files of the port
const Translation* translation(std::string_view id) {
    if (const auto it = g_loaded.find(id); it != g_loaded.end()) return it->second;
    const Translation* found = nullptr;
    for (const auto& [file_id, path] : language_files()) {
        if (file_id != id) continue;
        try {
            std::vector<std::string> warnings;
            g_read.push_back(std::make_unique<Translation>(Translation::load(path, &warnings)));
            for (const std::string& w : warnings) std::fprintf(stderr, "%s: %s\n", path.string().c_str(), w.c_str());
            found = g_read.back().get();
        } catch (const std::exception& e) {
            std::fprintf(stderr, "%s\n", e.what());
        }
        break;
    }
    g_loaded.emplace(std::string(id), found);
    return found;
}

}  // namespace

Language text_language() { return g_language; }

void set_text_language(Language language) { g_language = language; }

// @orig none: language files of the port
const char* tr(const char* english) {
    const std::string_view id = !g_choice.id.empty()                ? std::string_view(g_choice.id)
                                : g_language == Language::English ? std::string_view("en")
                                                                  : std::string_view("fr");
    if (const Translation* t = translation(id)) {
        if (const char* text = t->find(english)) return text;
    }
    return english;
}

bool regret_data() { return g_regret; }

void set_regret_data(bool regret) { g_regret = regret; }

void set_languages_dir(const std::filesystem::path& dir) {
    g_languages_dir = dir;
    g_loaded.clear();
}

// @orig none: language files of the port
std::vector<LanguageChoice> language_choices() {
    std::vector<LanguageChoice> choices = {{"", "AUTO"}};
    for (const auto& [id, path] : language_files()) {
        try {
            choices.push_back({id, name_of(id, Translation::load(path))});
        } catch (const std::exception&) {
            // Unreadable: not offered.
        }
    }
    return choices;
}

// @orig none: language files of the port
void select_language(std::string_view id) {
    g_loaded.clear();
    g_choice = {"", "AUTO"};
    const std::string wanted = lower(id);
    if (wanted.empty()) return;
    if (const Translation* t = translation(wanted)) {
        g_choice = {wanted, name_of(wanted, *t)};
    } else {
        std::fprintf(stderr, "language '%s' not found in '%s'\n", wanted.c_str(), g_languages_dir.string().c_str());
    }
}

const LanguageChoice& selected_language() { return g_choice; }

}  // namespace crusader
