#include "core/data_info.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>

namespace crusader {

namespace {

// Entries (far pointers) of USECODE/UNKCOFF.DAT.
constexpr std::uintmax_t kFrenchUnkcoffSize = 309 * 4;
constexpr std::uintmax_t kUsUnkcoffSize = 311 * 4;
constexpr std::uintmax_t kV101UnkcoffSize = 308 * 4;
constexpr std::uintmax_t kRegretUnkcoffSize = 350 * 4;
// Entry 319 of the German No Regret table: I_numToStr, 13F0:0073 (offset,
// then segment number 127); the English table has I_playSFX there.
constexpr std::uint16_t kGermanRegretEntry319[2] = {0x0073, 127};

// The German No Regret UNKCOFF.DAT (see kGermanRegretEntry319).
// @orig none: port-only data version detection
bool german_regret_unkcoff(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    unsigned char entry[4] = {};
    if (!in.seekg(319 * 4) || !in.read(reinterpret_cast<char*>(entry), 4)) return false;
    return (entry[0] | entry[1] << 8) == kGermanRegretEntry319[0] && (entry[2] | entry[3] << 8) == kGermanRegretEntry319[1];
}

// The version string of REGRET.EXE: "V1.06" between two NULs of the data
// segment (the banner "CRUSADER:  No Regret %s" takes it as an argument).
std::string regret_version(const std::string& bytes) {
    for (std::size_t at = bytes.find('V'); at != std::string::npos; at = bytes.find('V', at + 1)) {
        if (at == 0 || bytes[at - 1] != '\0') continue;
        std::size_t end = at + 1;
        while (end < bytes.size() && (std::isdigit(static_cast<unsigned char>(bytes[end])) || bytes[end] == '.')) ++end;
        if (end - at >= 4 && end < bytes.size() && bytes[end] == '\0' && bytes[at + 2] == '.') {
            return bytes.substr(at + 1, end - at - 1);
        }
    }
    return {};
}

}  // namespace

// @orig none: port-only data version detection
DataInfo detect_data_info(const std::filesystem::path& data) {
    DataInfo info;
    if (std::ifstream regret(data / "REGRET.EXE", std::ios::binary); regret) {
        const std::string bytes((std::istreambuf_iterator<char>(regret)), std::istreambuf_iterator<char>());
        if (bytes.find("No Regret") != std::string::npos) {
            info.regret = true;
            info.version = regret_version(bytes);
            // The joystick's message (DS:6A02 in the English executable).
            const bool german = bytes.find("[Beliebige Knopf pressen, um fortzufahren.]") != std::string::npos;
            info.language = german ? "DE" : "US";
            return info;
        }
    }
    std::ifstream in(data / "CRUSADER.EXE", std::ios::binary);
    if (!in) {
        // Without the executable, the size of the intrinsic table tells the
        // usecode: 309 entries in the French one, 311 in the US one, 308 in
        // the 1.01 one (its version known from it).
        std::error_code error;
        const std::uintmax_t size = std::filesystem::file_size(data / "USECODE/UNKCOFF.DAT", error);
        if (error) return info;
        if (size == kRegretUnkcoffSize) {
            info.regret = true;
            info.language = german_regret_unkcoff(data / "USECODE/UNKCOFF.DAT") ? "DE" : "US";
        } else if (size == kFrenchUnkcoffSize) info.language = "FR";
        else if (size == kUsUnkcoffSize) info.language = "US";
        else if (size == kV101UnkcoffSize) {
            info.language = "US";
            info.version = "1.01";
        }
        return info;
    }
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    constexpr std::string_view kBanner = "No Remorse V";
    for (std::size_t at = bytes.find(kBanner); at != std::string::npos && info.version.empty();
         at = bytes.find(kBanner, at + 1)) {
        std::size_t end = at + kBanner.size();
        const std::size_t begin = end;
        while (end < bytes.size() && (std::isdigit(static_cast<unsigned char>(bytes[end])) || bytes[end] == '.')) ++end;
        if (end > begin) info.version = bytes.substr(begin, end - begin);
    }
    // A message of the start-up code; the 1.01 executable has none, but its
    // English item names (DTABLE: family 12, record 5).
    struct Language {
        std::string_view marker, name;
    };
    constexpr Language kLanguages[] = {
        {"Appuyer sur une touche pour continuer.", "FR"},
        {"Press any key to continue.", "US"},
        {"SPIDER BOMB", "US"},
    };
    for (const Language& l : kLanguages) {
        if (bytes.find(l.marker) != std::string::npos) {
            info.language = l.name;
            break;
        }
    }
    return info;
}

}  // namespace crusader
