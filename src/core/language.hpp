#pragma once
// Language of the game data. The French and US releases ship their texts in
// CRUSADER.EXE (item names, messages, the shop of the weasel) and in the data
// files; the port prints the texts of the executable itself, so it keeps both
// versions and picks the one of the data in use. Game sets it when it loads
// the data (French until then); the port's own screens follow it too.

namespace crusader {

enum class Language { French, English };

Language text_language();
void set_text_language(Language language);

// The text of the current language.
inline const char* tr(const char* french, const char* english) {
    return text_language() == Language::English ? english : french;
}

// The data in use is the US version (the only English one supported): some
// screens of the US executable place their buttons for its English images.
inline bool us_version() { return text_language() == Language::English; }

}  // namespace crusader
