#include "core/language.hpp"

namespace crusader {

namespace {

Language g_language = Language::French;

}  // namespace

Language text_language() { return g_language; }

void set_text_language(Language language) { g_language = language; }

}  // namespace crusader
