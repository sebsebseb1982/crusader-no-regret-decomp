#pragma once
// No Regret: the executable is REGRET.EXE; its banner is "CRUSADER:  No Regret
// %s" and the version is a string of its own ("V1.06", DS:0B37 in the English
// 1.06 executable, "V1.07", DS:0B3B, in the German one); the language is told
// by the joystick's message, which each translation has. Without the
// executable, UNKCOFF.DAT has 350 entries, and its entry 319 tells the German
// table (I_numToStr, 13F0:0073) from the English one.
//
// The German 1.07 REGRET.EXE is the English 1.06 one translated: the same code
// and tables but for the places of its strings in the data segment. Its code
// differs only in the places of a few buttons, for its German pictures (main
// menu, options, the quick save and load questions), in the keyboard of the
// save names (QWERTZ and A O U umlauts), in the subtitles it reads (the
// "GERM" chunk of the movies' IFF instead of "ENGL"), in the violence being
// off unless CRUSADER.CFG says "violence=true" (its installer asks) and in
// the Christmas greeting it no longer prints. Its usecode is the English one
// with German texts, two signs set to other frames and a few lines of the
// computer terminals, but it numbers its intrinsics differently: English
// 0x153 is its 0x13F, English 0x13F..0x152 its 0x140..0x153
// (intrinsic_numbering.hpp). Nothing is cut from the game itself.
//
// What follows describes the No Remorse data, which this engine was first
// written for:
// Version and language of the game data, read from CRUSADER.EXE. The version
// is in the banner ("CRUSADER:  No Remorse V1.08"; DS:E8ED4 in the French 1.08
// executable, V1.23 in the US one, V1.01 in the first English one); the
// language is told by a text of the executable that exists in each
// translation. Without the executable, the size of the usecode's intrinsic
// table (USECODE/UNKCOFF.DAT) tells the French, US and 1.01 data apart.

#include "core/language.hpp"

#include <filesystem>
#include <string>

namespace crusader {

struct DataInfo {
    std::string version;   // "1.08", "1.23", "1.01"; No Regret "1.06", "1.07"; empty when unknown
    std::string language;  // "FR", "US" (English, 1.01 included) or "DE", empty when unknown
    bool regret = false;   // No Regret data (REGRET.EXE), else No Remorse

    bool known() const { return !language.empty(); }
    bool us() const { return language == "US"; }
    bool german() const { return language == "DE"; }
    // The English 1.01 data: its usecode numbers the intrinsics its own way.
    bool v101() const { return !regret && us() && version == "1.01"; }
    // The language of the texts: English for the US data, German for the
    // German data, else French.
    Language text_language() const {
        return us() ? Language::English : german() ? Language::German : Language::French;
    }
};

DataInfo detect_data_info(const std::filesystem::path& data);

}  // namespace crusader
