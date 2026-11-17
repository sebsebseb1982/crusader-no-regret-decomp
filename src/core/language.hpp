#pragma once
// Language of the game data. The French and US releases ship their texts in
// CRUSADER.EXE (item names, messages, the shop of the weasel) and in the data
// files; the port prints the texts of the executable itself, so it keeps both
// versions and picks the one of the data in use. Game sets it when it loads
// the data (French until then); the port's own screens follow it too.
//
// Port addition: the player may pick another language for those texts in the
// enhancements window ("language" in CRUSADER.CFG): the port's French or
// English ones whatever the data, or a language file of the "languages"
// folder (translation.hpp). The data's language still places the buttons
// (us_version).

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace crusader {

enum class Language { French, English };

Language text_language();
void set_text_language(Language language);

// The text of the language picked: the translation of `english` when a
// language file is in use and has one, else the port's French or English
// text. The pointer stays valid when another language is picked.
const char* tr(const char* french, const char* english);

// The data in use is the US version (the only English one supported): some
// screens of the US executable place their buttons for its English images.
inline bool us_version() { return text_language() == Language::English; }

// The data in use is No Regret's (set by Game with the language): its
// screens place their buttons differently.
bool regret_data();
void set_regret_data(bool regret);

// The languages offered: "" (the data's), "fr", "en", then the files of the
// languages folder ("<id>.txt", but template.txt; ids in lower case), by id.
struct LanguageChoice {
    std::string id;    // as written in CRUSADER.CFG
    std::string name;  // as shown (code page 850)
};
void set_languages_dir(const std::filesystem::path& dir);
std::vector<LanguageChoice> language_choices();
// Picks a language by id (any case); an unknown one, or a file that cannot be
// read, gives the data's (with a message on stderr). A file is read again
// each time it is picked, so a translator sees its changes.
void select_language(std::string_view id);
const LanguageChoice& selected_language();

}  // namespace crusader
