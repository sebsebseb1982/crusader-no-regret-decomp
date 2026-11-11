// Checks on the No Regret data (English 1.06, data/, not in the repository):
// the readers of its files and the start of a new game. Each check states a
// fact of that data; a fact that differs from No Remorse says so.
// Usage: regret_data_tests <data_dir> [--png DIR]

#include "core/anim_dat.hpp"
#include "core/data_info.hpp"
#include "core/dtable.hpp"
#include "core/flex.hpp"
#include "core/font.hpp"
#include "core/png_writer.hpp"
#include "core/shape.hpp"
#include "core/typeflags.hpp"
#include "engine/audio/audio.hpp"
#include "engine/usecode/intrinsic_numbering.hpp"
#include "game/game.hpp"

#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <utility>

using namespace crusader;

namespace {

int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                \
        }                                                              \
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
    const auto dir = std::filesystem::temp_directory_path() / "regret_tests";
    std::filesystem::create_directories(dir);
    return dir;
}

void test_detection(const std::filesystem::path& data) {
    const DataInfo info = detect_data_info(data);
    CHECK(info.regret && info.version == "1.06" && info.us() && !info.v101());
    // Without REGRET.EXE: told by the size of UNKCOFF.DAT, version unknown.
    const auto bare = std::filesystem::temp_directory_path() / "regret_no_exe";
    std::filesystem::create_directories(bare / "USECODE");
    std::filesystem::copy_file(data / "USECODE/UNKCOFF.DAT", bare / "USECODE/UNKCOFF.DAT",
                               std::filesystem::copy_options::overwrite_existing);
    const DataInfo bare_info = detect_data_info(bare);
    CHECK(bare_info.regret && bare_info.version.empty());

    // One number of the engine per entry of UNKCOFF.DAT (350 far pointers):
    // a French No Remorse number (below 309), or 0x400 + a No Regret number
    // for the 14 entries whose routine has no French number.
    const auto numbering = regret_intrinsic_numbering();
    CHECK(std::filesystem::file_size(data / "USECODE/UNKCOFF.DAT") == 4 * numbering.size());
    CHECK(intrinsic_numbering(info).data() == numbering.data());
    int own = 0;
    for (const std::uint16_t n : numbering) {
        CHECK(n < 309 || (n >= 0x400 && n < 0x400 + numbering.size()));
        own += n >= 0x400;
    }
    CHECK(own == 14);
    // Item::I_getFrame keeps its number; the timer tick moved from 0x0B4 to 0x14B.
    CHECK(numbering[0x001] == 0x001);
    CHECK(numbering[0x14B] == 0x0B4);
}

// Every frame of every shape decodes. SHAPES.FLX has two frames of garbage
// (shape 1122, frames 23 and 24: 33 bytes each, no valid header), which the
// reader turns into empty frames; No Remorse has none.
void test_shapes(const std::filesystem::path& data) {
    struct Archive {
        const char* file;
        ShapeLayout layout;
    };
    for (const Archive& a : {Archive{"STATIC/SHAPES.FLX", ShapeLayout::World}, Archive{"STATIC/GUMPS.FLX", ShapeLayout::Flat2D}}) {
        const auto flex = FlexArchive::open(data / a.file);
        std::size_t shapes = 0, frames = 0, errors = 0;
        std::set<std::pair<std::size_t, std::size_t>> bad;
        for (std::size_t i = 0; i < flex.count(); ++i) {
            const auto entry = flex.entry(i);
            if (entry.empty()) continue;
            try {
                const auto shape = Shape::parse(entry, a.layout, true);
                for (std::size_t k = 0; k < shape.frames().size(); ++k) {
                    const auto& f = shape.frames()[k];
                    Surface8 s(f.width + 2, f.height + 2);
                    f.draw(s, f.xoff + 1, f.yoff + 1);
                    f.draw(s, f.xoff + 1, f.yoff + 1, true);
                    ++frames;
                }
                if (shape.bad_frames() != 0) {
                    const auto strict = [&](std::size_t k) {
                        return shape.frames()[k].width == 0 && shape.frames()[k].height == 0 && shape.frames()[k].lines.empty();
                    };
                    for (std::size_t k = 0; k < shape.frames().size(); ++k) {
                        if (strict(k)) bad.insert({i, k});
                    }
                }
                ++shapes;
            } catch (const std::exception& e) {
                if (errors++ < 5) std::fprintf(stderr, "  %s #%zu: %s\n", a.file, i, e.what());
            }
        }
        std::printf("%s: %zu shapes, %zu frames, %zu errors, %zu unreadable frames\n", a.file, shapes, frames, errors, bad.size());
        CHECK(errors == 0);
        if (a.layout == ShapeLayout::World) {
            CHECK(shapes == 1641 && frames == 35578);
            const std::set<std::pair<std::size_t, std::size_t>> expected = {{1122, 23}, {1122, 24}};
            CHECK(bad == expected);
        } else {
            CHECK(shapes == 101 && frames == 294 && bad.empty());
        }
    }
    // Fonts: two more than No Remorse (8 and 18); font 2 has the same 128
    // frames stored with an end offset instead of a length.
    const auto fonts = FontSet::load(data / "STATIC/FONTS.FLX");
    CHECK(fonts.bad_frames() == 128);
    CHECK(fonts.glyph(8, 'A') && fonts.glyph(18, 'A'));
}

void test_tables(const std::filesystem::path& data) {
    const auto types = load_typeflags(data / "STATIC/TYPEFLAG.DAT");
    CHECK(types.size() == 2048);
    const auto anims = AnimDat::load(data / "STATIC/ANIM.DAT");
    std::size_t animated = 0;
    for (std::uint16_t s = 0; s < AnimDat::kShapes; ++s) animated += anims.has_shape(s);
    std::printf("ANIM.DAT: %zu animated shapes, %zu actions\n", animated, anims.action_count());
    // No Remorse: 41 animated shapes, 357 actions.
    CHECK(animated == 60 && anims.action_count() == 476);
}

// A new game: the first map loads, the scripts run without fault and every
// intrinsic they call is bound.
void test_new_game(const std::filesystem::path& data) {
    Game game(data, saves_dir());
    CHECK(game.data_version() == "1.06");
    // The tables of REGRET.EXE: the new weapons, their ammunition and fire types.
    CHECK(std::string(dtable::weapon_name(0x5F6)) == "BK-16" && std::string(dtable::weapon_name(0x198)) == "XP-5");
    CHECK(dtable::ammo_shape(0x5F6) == 1557 && dtable::clip_size(0x5F6) == 12);
    CHECK(std::string(dtable::item_name(1261)) == "VIR IMAGER" && dtable::item_icon(1261) == 32);
    CHECK(dtable::fire_type(0x16) != nullptr && dtable::fire_type(0x17) == nullptr);
    CHECK(dtable::fire_type(5)->min_damage() == 45 && dtable::fire_type(5)->max_damage() == 60);
    CHECK(game.world().equip_slots() == 45);
    game.new_game(0);
    World& w = game.world();
    game.update(kFrameUs);
    const Item* player = w.get(1);
    std::printf("new game: map %d, player at (%d, %d, %d), movie %d\n", w.current_map(), player ? player->x : -1,
                player ? player->y : -1, player ? player->z : -1, game.movie() ? 1 : 0);
    CHECK(player != nullptr);
    if (game.movie()) {
        game.key(0x01, false, true, 0, 0x1B);
        for (int i = 0; game.movie() && i < 200; ++i) game.update(kFrameUs);
        game.key(0x01, false, false, 0);
    }
    for (int i = 0; i < 1800; ++i) game.update(kFrameUs);
    shot(game, "new_game.png");
    player = w.get(1);
    std::printf("after 30 s: map %d, player at (%d, %d, %d)\n", w.current_map(), player->x, player->y, player->z);
    // The mission's music: the track of map 1 ("ninth"), asked as track 0x45.
    if (Audio* audio = w.audio()) {
        std::printf("music: track %d\n", audio->music.current());
        CHECK(audio->music.current() == Music::map_track(w.current_map()) && Music::map_track(1) == 1);
        CHECK(audio->music.file_name(1) == std::optional<std::string>("ninth"));
    }
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

// Game_start (1030:032D): ORIGIN and ANIM01, then the menu with its palette
// (MISC.PAL), not the black the movies leave nor the game's cycling colours.
void test_start_up(const std::filesystem::path& data) {
    Game game(data, saves_dir());
    game.start_up();
    CHECK(game.menu() != nullptr && game.in_movie());
    for (int m = 0; game.in_movie() && m < 4; ++m) {
        game.key(0x01, false, true, 0, 0x1B);
        for (int i = 0; game.in_movie() && i < 200; ++i) game.update(kFrameUs);
        game.key(0x01, false, false, 0);
    }
    CHECK(!game.in_movie());
    for (int i = 0; i < 120; ++i) game.update(kFrameUs);
    shot(game, "start_up_menu.png");
    CHECK(game.menu() != nullptr);
    CHECK(game.palette() == Palette::load(data / "STATIC/MISC.PAL"));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2 || !std::filesystem::exists(std::filesystem::path(argv[1]) / "STATIC")) {
        std::printf("regret_data_tests: no game data, skipped\n");
        return 0;
    }
    const std::filesystem::path data = argv[1];
    for (int i = 2; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--png") g_png_dir = argv[++i];
    }
    try {
        test_detection(data);
        test_shapes(data);
        test_tables(data);
        test_new_game(data);
        test_start_up(data);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: exception: %s\n", e.what());
        ++failures;
    }
    if (failures != 0) std::fprintf(stderr, "%d failure(s)\n", failures);
    else std::printf("regret_data_tests: ok\n");
    return failures == 0 ? 0 : 1;
}
