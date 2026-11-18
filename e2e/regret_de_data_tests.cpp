// Checks on the German No Regret data (1.07, data_de/, not in the repository):
// the English 1.06 executable translated, its usecode numbering its
// intrinsics another way (data_info.hpp). Each check states a fact of that
// data that differs from the English one, then a new game runs its scripts.
// Usage: regret_de_data_tests <data_dir> [--png DIR]

#include "core/data_info.hpp"
#include "core/flex.hpp"
#include "core/language.hpp"
#include "core/movie_subtitles.hpp"
#include "core/png_writer.hpp"
#include "engine/usecode/intrinsic_numbering.hpp"
#include "engine/usecode/usecode_file.hpp"
#include "game/game.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace crusader;

namespace {

int failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

constexpr std::uint32_t kFrameUs = 1'000'000 / 60;

std::filesystem::path g_png_dir;

void shot(Game& game, const char* name) {
    if (g_png_dir.empty()) return;
    Surface8 screen(Game::kScreenWidth, Game::kScreenHeight);
    game.render(screen);
    write_png(g_png_dir / name, screen, game.palette());
}

// The game writes nothing in the data folder: the tests only start new games.
std::filesystem::path saves_dir() {
    const auto dir = std::filesystem::temp_directory_path() / "regret_de_tests";
    std::filesystem::create_directories(dir);
    return dir;
}

// Version 1.07 and the German joystick message; without REGRET.EXE, entry
// 319 of UNKCOFF.DAT.
void test_detection(const std::filesystem::path& data) {
    const DataInfo info = detect_data_info(data);
    CHECK(info.regret && info.version == "1.07" && info.german() && info.text_language() == Language::German);
    const auto bare = std::filesystem::temp_directory_path() / "regret_de_no_exe";
    std::filesystem::create_directories(bare / "USECODE");
    std::filesystem::copy_file(data / "USECODE/UNKCOFF.DAT", bare / "USECODE/UNKCOFF.DAT",
                               std::filesystem::copy_options::overwrite_existing);
    const DataInfo bare_info = detect_data_info(bare);
    CHECK(bare_info.regret && bare_info.german() && bare_info.version.empty());
}

// The German numbering gives one engine number per routine of UNKCOFF.DAT:
// entries naming the same far pointer get the same number, different ones
// different numbers, as in the English table it is made from.
void test_numbering(const std::filesystem::path& data) {
    const auto numbering = german_regret_intrinsic_numbering();
    CHECK(intrinsic_numbering(detect_data_info(data)).data() == numbering.data());
    std::ifstream in(data / "USECODE/UNKCOFF.DAT", std::ios::binary);
    std::vector<std::uint32_t> pointers;
    for (unsigned char e[4]; in.read(reinterpret_cast<char*>(e), 4);) {
        pointers.push_back(e[0] | e[1] << 8 | e[2] << 16 | static_cast<std::uint32_t>(e[3]) << 24);
    }
    CHECK(pointers.size() == numbering.size());
    std::map<std::uint32_t, std::uint16_t> by_pointer;
    std::map<std::uint16_t, std::uint32_t> by_number;
    for (std::size_t i = 0; i < pointers.size() && i < numbering.size(); ++i) {
        const auto [p, new_pointer] = by_pointer.emplace(pointers[i], numbering[i]);
        const auto [n, new_number] = by_number.emplace(numbering[i], pointers[i]);
        if (p->second != numbering[i] || n->second != pointers[i]) {
            std::fprintf(stderr, "entry 0x%03zX: routine and number disagree\n", i);
            ++failures;
        }
    }
    // 0x13F: UCMachine::I_numToStr, 13F0:0073 (segment 127), French 0x0FF.
    CHECK(pointers.size() > 0x13F && pointers[0x13F] == (127u << 16 | 0x0073) && numbering[0x13F] == 0x0FF);

    // The usecode tells itself German: class 0x0A1D is TEXTFIL3.
    const UsecodeFile usecode = UsecodeFile::from_flex(FlexArchive::open(data / "USECODE/EUSECODE.FLX"));
    CHECK(german_regret_usecode(usecode));
}

// The movies' subtitles: the German executable reads the "GERM" chunk.
void test_subtitles(const std::filesystem::path& data) {
    const std::vector<RegretSubtitle> german = load_regret_movie_subtitles(data / "FLICS/01A.IFF", "GERM");
    CHECK(!german.empty() && german[0].text.rfind("Ich hoff", 0) == 0);
}

// A new game: German texts, the violence off without CRUSADER.CFG
// ("VIOLENCE=0"), the first map loaded and its scripts run without fault,
// every intrinsic they call bound.
void test_new_game(const std::filesystem::path& data) {
    Game game(data, saves_dir());
    CHECK(game.data_version() == "1.07" && game.data_language() == "DE");
    CHECK(german_version() && regret_data());
    CHECK(std::string(tr("DATA LINK")) == "DATENSCHIRM" && std::string(tr("MESSAGE")) == "NACHRICHT");
    game.new_game(0);
    World& w = game.world();
    if (!std::filesystem::exists(data / "CRUSADER.CFG")) CHECK(!w.state.violence);
    game.update(kFrameUs);
    if (game.movie()) {
        game.key(0x01, false, true, 0, 0x1B);
        for (int i = 0; game.movie() && i < 200; ++i) game.update(kFrameUs);
        game.key(0x01, false, false, 0);
    }
    for (int i = 0; i < 1800; ++i) game.update(kFrameUs);
    shot(game, "de_new_game.png");
    const Item* player = w.get(1);
    CHECK(player != nullptr && w.current_map() == 1);
    // The No Regret only intrinsic not written yet: the Xan movie (0x06D).
    const std::set<std::uint16_t> todo = {0x46D};
    bool unknown_unbound = false;
    for (const auto& [number, calls] : game.machine().unbound_intrinsic_calls()) {
        if (todo.count(number)) continue;
        std::fprintf(stderr, "unbound intrinsic 0x%03X: %u calls\n", number, calls);
        unknown_unbound = true;
    }
    for (const auto& f : game.machine().faults()) std::fprintf(stderr, "fault: %s\n", f.message.c_str());
    CHECK(game.machine().faults().empty());
    CHECK(!unknown_unbound);
}

}  // namespace

int main(int argc, char** argv) {
    set_languages_dir(CRUSADER_LANGUAGES_DIR);  // the German texts (languages/de/port.txt)
    if (argc < 2 || !std::filesystem::exists(std::filesystem::path(argv[1]) / "STATIC")) {
        std::printf("regret_de_data_tests: no game data, skipped\n");
        return 0;
    }
    const std::filesystem::path data = argv[1];
    for (int i = 2; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--png") g_png_dir = argv[++i];
    }
    try {
        test_detection(data);
        test_numbering(data);
        test_subtitles(data);
        test_new_game(data);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: exception: %s\n", e.what());
        ++failures;
    }
    if (failures != 0) std::fprintf(stderr, "%d failure(s)\n", failures);
    else std::printf("regret_de_data_tests: ok\n");
    return failures == 0 ? 0 : 1;
}
