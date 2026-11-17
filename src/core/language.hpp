#pragma once
// Language of the game data. The French and US releases ship their texts in
// CRUSADER.EXE (item names, messages, the shop of the weasel) and in the data
// files; the port prints the texts of the executable itself. Game sets the
// language when it loads the data (French until then).
//
// Port addition: those texts, and the ones of the port's own screens, are
// written in English in the code (tr("BACK")) and translated by the language
// files of the "languages" folder (translation.hpp; docs/TRANSLATING.md):
// fr.txt holds the French executable's texts. The player picks the language
// in the enhancements window ("language" in CRUSADER.CFG): AUTO, the default,
// takes the data's (fr.txt or en.txt). The data's language still places the
// buttons (us_version).

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace crusader {

enum class Language { French, English };

Language text_language();
void set_text_language(Language language);

// The translation of an English text in the language in use: the text itself
// when the language file has none (or there is no file). The pointer stays
// valid when another language is picked.
const char* tr(const char* english);

// The level editor's own texts, which are not in the language files: the
// French or the English one, after the data's language.
inline const char* tr(const char* french, const char* english) {
    return text_language() == Language::English ? english : french;
}

// The data in use is the US version (the only English one supported): some
// screens of the US executable place their buttons for its English images.
inline bool us_version() { return text_language() == Language::English; }

// The data in use is No Regret's (set by Game with the language): its
// screens place their buttons differently.
bool regret_data();
void set_regret_data(bool regret);

// The languages offered: "" (AUTO, the data's), then the files of the
// languages folder ("<id>.txt", ids in lower case), by id.
struct LanguageChoice {
    std::string id;    // as written in CRUSADER.CFG
    std::string name;  // as shown (code page 850): the file's "@name"
};
void set_languages_dir(const std::filesystem::path& dir);
std::vector<LanguageChoice> language_choices();
// Picks a language by id (any case); an unknown one, or a file that cannot be
// read, gives AUTO (with a message on stderr). The files are read again after
// each pick, so a translator sees the changes.
void select_language(std::string_view id);
const LanguageChoice& selected_language();

}  // namespace crusader
