#include "core/language.hpp"

namespace crusader {

namespace {

Language g_language = Language::French;
bool g_regret = false;

}  // namespace

Language text_language() { return g_language; }

void set_text_language(Language language) { g_language = language; }

bool regret_data() { return g_regret; }

void set_regret_data(bool regret) { g_regret = regret; }

}  // namespace crusader
