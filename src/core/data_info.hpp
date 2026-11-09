#pragma once
// No Regret: the executable is REGRET.EXE; its banner is "CRUSADER:  No Regret
// %s" and the version is a string of its own ("V1.06", DS:0B37 in the English
// 1.06 executable). Without the executable, UNKCOFF.DAT has 350 entries.
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
    std::string version;   // "1.08", "1.23", "1.01"; empty when unknown
    std::string language;  // "FR" or "US" (English, 1.01 included), empty when unknown
    bool regret = false;   // No Regret data (REGRET.EXE), else No Remorse

    bool known() const { return !language.empty(); }
    bool us() const { return language == "US"; }
    // The English 1.01 data: its usecode numbers the intrinsics its own way.
    bool v101() const { return !regret && us() && version == "1.01"; }
    // The language of the texts: English for the US data, else French.
    Language text_language() const { return us() ? Language::English : Language::French; }
};

DataInfo detect_data_info(const std::filesystem::path& data);

}  // namespace crusader
