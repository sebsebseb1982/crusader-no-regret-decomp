#include "core/language.hpp"

#include "core/translation.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>
#include <system_error>

namespace crusader {

namespace {

Language g_language = Language::French;
bool g_regret = false;

std::filesystem::path g_languages_dir;
LanguageChoice g_choice{"", "AUTO"};
// The translations read, kept so that the texts handed out stay valid; the
// last one is in use when a file is picked.
std::vector<std::unique_ptr<Translation>> g_translations;
const Translation* g_translation = nullptr;

constexpr std::string_view kTemplate = "template";

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
        std::string id = lower(path.stem().string());
        if (id == kTemplate || id == "fr" || id == "en") continue;
        files.emplace_back(std::move(id), path);
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

}  // namespace

Language text_language() { return g_language; }

void set_text_language(Language language) { g_language = language; }

// @orig none: language files of the port
const char* tr(const char* french, const char* english) {
    if (g_translation) {
        if (const char* text = g_translation->find(english)) return text;
    }
    const Language language = g_choice.id == "fr"   ? Language::French
                              : g_choice.id == "en" ? Language::English
                                                    : g_language;
    return language == Language::English ? english : french;
}

bool regret_data() { return g_regret; }

void set_regret_data(bool regret) { g_regret = regret; }

void set_languages_dir(const std::filesystem::path& dir) { g_languages_dir = dir; }

// @orig none: language files of the port
std::vector<LanguageChoice> language_choices() {
    std::vector<LanguageChoice> choices = {{"", "AUTO"}, {"fr", "FRANCAIS"}, {"en", "ENGLISH"}};
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
    const std::string wanted = lower(id);
    g_translation = nullptr;
    g_choice = {"", "AUTO"};
    if (wanted.empty()) return;
    if (wanted == "fr" || wanted == "en") {
        g_choice = {wanted, wanted == "fr" ? "FRANCAIS" : "ENGLISH"};
        return;
    }
    for (const auto& [file_id, path] : language_files()) {
        if (file_id != wanted) continue;
        try {
            std::vector<std::string> warnings;
            auto t = std::make_unique<Translation>(Translation::load(path, &warnings));
            for (const std::string& w : warnings) std::fprintf(stderr, "%s: %s\n", path.string().c_str(), w.c_str());
            g_choice = {file_id, name_of(file_id, *t)};
            g_translation = t.get();
            g_translations.push_back(std::move(t));
        } catch (const std::exception& e) {
            std::fprintf(stderr, "%s\n", e.what());
        }
        return;
    }
    std::fprintf(stderr, "language '%s' not found in '%s'\n", wanted.c_str(), g_languages_dir.string().c_str());
}

const LanguageChoice& selected_language() { return g_choice; }

}  // namespace crusader
