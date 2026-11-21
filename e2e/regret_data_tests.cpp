// Checks on the No Regret data (English 1.06, data/, not in the repository):
// the readers of its files and the start of a new game. Each check states a
// fact of that data; a fact that differs from No Remorse says so.
// Usage: regret_data_tests <data_dir> [--png DIR]

#include "core/anim_dat.hpp"
#include "core/data_info.hpp"
#include "core/dtable.hpp"
#include "core/flex.hpp"
#include "core/font.hpp"
#include "core/language.hpp"
#include "core/palette.hpp"
#include "core/png_writer.hpp"
#include "core/shape.hpp"
#include "core/typeflags.hpp"
#include "core/xform.hpp"
#include "engine/audio/audio.hpp"
#include "engine/ui/gump_keys.hpp"
#include "engine/ui/help_screen.hpp"
#include "engine/ui/keypad_gump.hpp"
#include "engine/ui/load_screen.hpp"
#include "engine/usecode/intrinsic_numbering.hpp"
#include "engine/usecode/world_intrinsics.hpp"
#include "engine/world/actor_anim.hpp"
#include "engine/world/collision.hpp"
#include "engine/world/inventory.hpp"
#include "engine/world/lighting.hpp"
#include "engine/world/render.hpp"
#include "engine/world/projectile.hpp"
#include "game/game.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

void shot_surface(const Surface8& screen, const Palette& palette, const char* name) {
    if (!g_png_dir.empty()) write_png(g_png_dir / name, screen, palette);
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

// Status 0x1000 saved while a process of the item's class ran (the
// processes are not loaded): a security monitor (WATCHNS, WATCHEW) in use
// gets it cleared, or use() would do nothing (SEB4), and so does a keypad
// (KEYPADNS, KEYPADEW) saved during its KEYPAD script (IMPOSSIBLECODE: use()
// did nothing, the S selection skipped it); a running timer (TIMER)
// and a rolling roller (ROLLB_NS) get their loop started again (events 0x20
// and 0x21), which keeps the bit. A one-shot timer (QHi bit 0 clear) clears
// it itself once its delay is over: it runs, as it never would without.
void test_saved_busy_items(const std::filesystem::path& data) {
    Game game(data, saves_dir());
    game.new_game(0);
    game.update(kFrameUs);
    World& w = game.world();
    const UsecodeFile& uc = game.machine().usecode();
    ObjId monitor = 0, timer = 0, roller = 0, keypad = 0;
    int delay = 0;
    w.for_each([&](const Item& i) {
        if (i.parent || !uc.has_class(i.shape)) return;
        const std::string cls = uc.class_name(i.shape);
        if (!monitor && (cls == "WATCHNS" || cls == "WATCHEW")) monitor = i.id;
        if (!keypad && (cls == "KEYPADNS" || cls == "KEYPADEW")) keypad = i.id;
        // The delay: map array * 256 + NPC number (TIMER_00C0); none ends
        // at once.
        const int d = i.map_number * 256 + i.npc_number;
        if (!timer && cls == "TIMER" && d > 0 && d < 600 && !((i.quality >> 8) & 1)) timer = i.id, delay = d;
        if (!roller && (cls == "ROLLB_NS" || cls == "ROLLB_EW")) roller = i.id;
    });
    std::printf("saved busy: monitor %u, keypad %u, timer %u (delay %d), roller %u\n", monitor, keypad, timer, delay,
                roller);
    CHECK(monitor != 0 && keypad != 0 && timer != 0 && roller != 0);
    if (!monitor || !keypad || !timer || !roller) return;
    for (ObjId id : {monitor, keypad, timer, roller}) w.get(id)->flags |= 0x1000;
    game.save_game(LoadScreen::kNames);
    game.load_save_file(saves_dir() / ("SAVEGAME." + std::to_string(LoadScreen::kNames)));
    World& loaded = game.world();
    CHECK(loaded.get(monitor) != nullptr && !(loaded.get(monitor)->flags & 0x1000));
    CHECK(loaded.get(keypad) != nullptr && !(loaded.get(keypad)->flags & 0x1000));
    const auto runs = [&](ObjId id) {
        int n = 0;
        game.kernel().for_each_process([&](const Process& p) { n += p.owner() == id; });
        return n;
    };
    CHECK(runs(roller) >= 1 && (loaded.get(roller)->flags & 0x1000));
    CHECK(loaded.get(timer)->flags & 0x1000);
    for (int i = 0; i < 4 * delay + 60 && (loaded.get(timer)->flags & 0x1000); ++i) game.update(kFrameUs);
    CHECK(!(loaded.get(timer)->flags & 0x1000));
}

// The autosave of the right code (port addition) waits for the end of the
// KEYPAD script: taken when the keypad closed, it held the keypad's busy
// bit (0x1000) and a door not yet opened, so once loaded the keypad was
// dead and the door shut for good (IMPOSSIBLECODE). Saved afterwards, the
// keypad is solved (status 4) and free.
void test_keypad_autosave(const std::filesystem::path& data) {
    Game game(data, saves_dir());
    game.config().autosave = true;
    game.new_game(0);
    game.update(kFrameUs);
    World& w = game.world();
    const UsecodeFile& uc = game.machine().usecode();
    ObjId keypad = 0;
    w.for_each([&](const Item& i) {
        if (keypad || i.parent || !uc.has_class(i.shape) || i.map_number == 0) return;
        const std::string cls = uc.class_name(i.shape);
        if (cls == "KEYPADNS" || cls == "KEYPADEW") keypad = i.id;
    });
    CHECK(keypad != 0);
    if (!keypad) return;
    const auto autosave = saves_dir() / ("SAVEGAME." + std::to_string(Game::kAutoSave));
    std::filesystem::remove(autosave);
    use_item(w, keypad);
    for (int f = 0; f < 3; ++f) game.update(kFrameUs);
    const auto* gump = dynamic_cast<const KeypadGump*>(game.screen());
    CHECK(gump != nullptr);
    if (!gump) return;
    const int code = gump->code();
    static constexpr std::uint8_t kDigitScancodes[10] = {0x0B, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
    const auto press = [&game](std::uint8_t scancode, std::uint8_t ascii) {
        game.key(scancode, false, true, 0, ascii);
        game.key(scancode, false, false, 0, ascii);
        game.update(kFrameUs);
    };
    for (const char c : std::to_string(code)) press(kDigitScancodes[c - '0'], static_cast<std::uint8_t>(c));
    press(0x1C, 0x0D);
    std::printf("keypad autosave: keypad %u, code %d, closed %d, flags %04X\n", keypad, code, game.screen() == nullptr,
                w.get(keypad)->flags);
    CHECK(!game.screen() && (w.get(keypad)->flags & 0x1000) && !std::filesystem::exists(autosave));
    for (int f = 0; f < 600 && !std::filesystem::exists(autosave); ++f) game.update(kFrameUs);
    CHECK(std::filesystem::exists(autosave));
    if (!std::filesystem::exists(autosave)) return;
    game.load_save(Game::kAutoSave);
    const std::uint16_t flags = game.world().get(keypad)->flags;
    std::printf("keypad autosave: loaded flags %04X\n", flags);
    CHECK(!(flags & 0x1000) && (flags & 0x0004));
}

// The datalink (DATALINK use()) hides the status bar, then on some maps has
// FLICTEST play a movie and shows nothing again: the movie intrinsic
// (1448:0B5A) does, its WORLD_1008_0ff1(1) shows the bar whatever its state
// before. During the movie the bar is hidden; afterwards the cursor is too.
void test_movie_shows_status_bar(const std::filesystem::path& data) {
    Game game(data, saves_dir());
    game.new_game(0);
    for (int i = 0; i < 300 && game.in_movie(); ++i) game.update(kFrameUs);
    World& w = game.world();
    CHECK(!game.in_movie() && w.state.status_bar_visible);
    w.state.status_bar_visible = false;  // CruStatusGump::I_hideStatusGump
    game.play_movie("1a", MovieKind::kCutscene);
    CHECK(game.in_movie() && !w.state.status_bar_visible && !game.cursor().visible());
    game.update(kFrameUs);  // the first frame: a key before it is not seen
    game.key(0x01, false, true, 0, 0x1B);
    for (int i = 0; game.in_movie() && i < 200; ++i) game.update(kFrameUs);
    game.key(0x01, false, false, 0);
    CHECK(!game.in_movie());
    CHECK(w.state.status_bar_visible);
    CHECK(!game.cursor().visible());
}

// A shot at a floor mine (FLORMINE, shape 0x29F, 1 high, lying 1 above the
// floor) whose spread takes it down to z 0 under the mine: the step that
// ends at z 0 is swept in parts of 16 (SPRITE_1150_123B), meets the mine on
// the way down and blows it up (shape 0x58). The port ended such a step on
// nothing, unswept: mines on a floor at z 0 could not be shot.
void test_shot_floor_mine(const std::filesystem::path& data) {
    Game game(data, saves_dir());
    game.new_game(0);
    for (int i = 0; i < 300 && game.in_movie(); ++i) game.update(kFrameUs);
    World& w = game.world();
    const Item* player = w.get(1);
    const ObjId mine = w.create(0x29F, 0);
    CHECK(mine != 0);
    if (!mine) return;
    // A place with a clear line from the player's chest to the mine.
    const World::Point start{player->x, player->y, player->z + 40};
    World::Point target{};
    bool clear = false;
    for (int dir = 0; dir < 16 && !clear; dir += 2) {
        for (int distance = 160; distance >= 64 && !clear; distance -= 32) {
            w.move_item(mine, player->x + kDirectionDx[dir] * distance, player->y + kDirectionDy[dir] * distance,
                        player->z + 1);
            target = aim_point(w, 1, mine);
            const SweepResult line = sweep(w, kProjectileShape, 1, start, target, kSweepNoGround);
            clear = line.ok && line.blocker == 0;
        }
    }
    CHECK(clear);
    target.z = 0;
    game.kernel().add(std::make_unique<ProjectileProcess>(w, game.animations(), 1, start, target, 2, 20, 0));
    for (int i = 0; i < 120 && game.kernel().count_processes(0, ProjectileProcess::kType) != 0; ++i)
        game.update(kFrameUs);
    const Item* after = w.get(mine);
    std::printf("floor mine: player z %d, aimed at (%d, %d, 0), mine's shape now %X\n", player->z, target.x,
                target.y, after ? after->shape : 0);
    CHECK(game.kernel().count_processes(0, ProjectileProcess::kType) == 0);
    CHECK(!after || after->shape != 0x29F);
}

// Port lighting: No Regret's lantern (wall lamp 0x2A0 frame 8; 0..9 whole,
// 10..19 broken) put by the player in a new game lights from its bulb, and
// goes out once broken (frame 18).
void test_fixture_lights(const std::filesystem::path& data) {
    Game game(data, saves_dir());
    game.new_game(0);
    for (int i = 0; i < 300 && game.in_movie(); ++i) game.update(kFrameUs);
    World& w = game.world();
    CHECK(!w.remorse());
    const Item* player = w.get(1);
    const ObjId lamp = w.create(0x2A0, 8);
    CHECK(lamp != 0 && player);
    if (!lamp || !player) return;
    w.move_item(lamp, player->x + 64, player->y + 64, player->z + 40);
    const auto xform = XformTable::load(data / "STATIC/XFORMPAL.DAT");
    WorldLighting lighting(game.shapes(), xform);
    auto lamp_lights = [&] {
        const Camera camera{world_from_usecode(player->x), world_from_usecode(player->y), player->z};
        const int left = ((camera.x - camera.y) >> 2) - 320, top = (((camera.x + camera.y) >> 3) - camera.z) - 240;
        const auto list = build_display_list(w, game.shapes(), View{left, top}, 640, 480);
        Surface8 screen(640, 480);
        draw_display_list(screen, list, &xform);
        const LightingOptions opt;
        lighting.capture(w, list, screen, left, top, game.palette(), opt, 0);
        std::vector<std::uint32_t> argb(640 * 480);
        lighting.shade(w, screen, game.palette(), opt, {}, argb.data());
        int n = 0;
        for (const PointLight& l : lighting.lights()) n += l.owner == lamp;
        return n;
    };
    const int lit = lamp_lights();
    w.get(lamp)->frame = 18;  // broken
    const int broken = lamp_lights();
    std::printf("lantern: %d light(s) whole, %d broken\n", lit, broken);
    CHECK(lit == 1 && broken == 0);
}

// A map of FIXED.DAT as the lighting sees it, without the game: its items
// found by shape and place (world units, as the map editor shows them).
struct LitMap {
    std::vector<ShapeInfo> typeflags;
    World w;
    FlexArchive shapes_flex;
    ShapeCache shapes;
    XformTable xform;
    Palette palette;
    WorldLighting lighting;

    LitMap(const std::filesystem::path& data, std::size_t map)
        : typeflags(load_typeflags(data / "STATIC/TYPEFLAG.DAT")),
          w(typeflags),
          shapes_flex(FlexArchive::open(data / "STATIC/SHAPES.FLX")),
          shapes(shapes_flex, ShapeLayout::World),
          xform(XformTable::load(data / "STATIC/XFORMPAL.DAT")),
          palette(Palette::load(data / "STATIC/GAMEPAL.PAL")),
          lighting(shapes, xform) {
        w.load_fixed_map(FlexArchive::open(data / "STATIC/FIXED.DAT"), map);
        w.expand_glob_eggs(FlexArchive::open(data / "STATIC/GLOB.FLX"));
    }

    ObjId find(std::uint16_t shape, std::int32_t x, std::int32_t y) const {
        ObjId found = 0;
        w.for_each([&](const Item& i) {
            if (i.shape == shape && world_from_usecode(i.x) == x && world_from_usecode(i.y) == y) found = i.id;
        });
        return found;
    }

    // The lights of item `id`, and the puffs in view, on a 640x480 view
    // centred on it at game tick `ticks`.
    std::vector<PointLight> lights_of(ObjId id, std::uint32_t ticks = 0) {
        const Item& item = *w.get(id);
        const Camera camera{world_from_usecode(item.x), world_from_usecode(item.y), item.z};
        const int left = ((camera.x - camera.y) >> 2) - 320, top = (((camera.x + camera.y) >> 3) - camera.z) - 240;
        const auto list = build_display_list(w, shapes, View{left, top}, 640, 480);
        Surface8 screen(640, 480);
        draw_display_list(screen, list, &xform);
        const LightingOptions opt;
        lighting.capture(w, list, screen, left, top, palette, opt, ticks);
        std::vector<std::uint32_t> argb(640 * 480);
        lighting.shade(w, screen, palette, opt, {}, argb.data());
        std::vector<PointLight> out;
        for (const PointLight& l : lighting.lights())
            if (l.owner == id) out.push_back(l);
        return out;
    }
};

// Port lighting: No Regret has No Remorse's wall lamps 0xBC (map 2: the
// floodlight at 48062, 49246, frame 0; 1, 4 caged red lamps, 2, 5 red
// lanterns, 6..11 broken) and its alarm lights, and adds alert beacons. On
// map 1, the alarm box at 31038, 12286 (0x477, frame 1) is dark until an
// alert (frame 3), and so is the beacon at 20574, 12350 (0x663, frame 1),
// which an alert turns into the sweeping red one (0x660).
void test_wall_lamps_and_alarms(const std::filesystem::path& data) {
    {
        LitMap m(data, 2);
        const ObjId flood = m.find(0x0BC, 48062, 49246);
        CHECK(flood != 0);
        if (!flood) return;
        const auto white = m.lights_of(flood);
        m.w.get(flood)->frame = 1;
        const auto red = m.lights_of(flood);
        m.w.get(flood)->frame = 6;
        const auto broken = m.lights_of(flood);
        std::printf("wall lamp 0xBC: %zu light(s) as a floodlight, %zu as a red lamp, %zu broken\n", white.size(),
                    red.size(), broken.size());
        CHECK(white.size() == 1 && red.size() == 1 && broken.empty());
        if (white.size() == 1) CHECK(white[0].color.z >= white[0].color.x);
        if (red.size() == 1) CHECK(red[0].color.x > 2 * red[0].color.z);
    }
    LitMap m(data, 1);
    const ObjId box = m.find(0x477, 31038, 12286), beacon = m.find(0x663, 20574, 12350);
    CHECK(box != 0 && beacon != 0);
    if (!box || !beacon) return;
    CHECK(m.lights_of(box).empty() && m.lights_of(beacon).empty());
    set_alert(m.w, true);
    CHECK(m.w.get(box)->frame == 3 && m.w.get(beacon)->shape == 0x660);
    const auto box_lit = m.lights_of(box, 8), beacon_lit = m.lights_of(beacon, 8);
    std::printf("alert: alarm box %zu light(s), beacon %zu\n", box_lit.size(), beacon_lit.size());
    CHECK(box_lit.size() == 1 && beacon_lit.size() == 1);
    if (beacon_lit.size() == 1) CHECK(beacon_lit[0].color.x > 4 * beacon_lit[0].color.y);
}

// Port lighting: No Regret's own lamps light, and go out when broken: the
// egg-shaped lamp of map 19, the standard lamp and the desk lamp of map 6,
// the red globe of map 5.
void test_regret_lamps(const std::filesystem::path& data) {
    struct Lamp {
        std::size_t map;
        std::uint16_t shape;
        std::int32_t x, y;
        std::uint16_t broken;
    };
    const Lamp lamps[] = {
        {19, 0x0BE, 54334, 53246, 2}, {6, 0x407, 5214, 5726, 3}, {6, 0x414, 4094, 13310, 3}, {5, 0x493, 30014, 22558, 2}};
    for (const Lamp& l : lamps) {
        LitMap m(data, l.map);
        const ObjId id = m.find(l.shape, l.x, l.y);
        CHECK(id != 0);
        if (!id) continue;
        const std::size_t lit = m.lights_of(id).size();
        m.w.get(id)->frame = l.broken;
        const std::size_t broken = m.lights_of(id).size();
        std::printf("lamp 0x%X: %zu light(s) whole, %zu broken\n", l.shape, lit, broken);
        CHECK(lit == 1 && broken == 0);
    }
}

// Port addition, the controller: running one way, the aim stick swung behind
// him, the left stick still held the same way. The gamepad then sends the
// strafe away from the aim (Alt + down, a step back), relative to the aim; a
// step goes by his own direction. No Regret cuts the step a held key repeats
// into and plays it again at once (1148:1A03), so he stayed busy, never
// turned to the aim, and backed towards it facing away from it, against the
// stick. The aim turns the steps as they go (aim_mode.cpp, aim_at_angle).
// The arrival of a new game leaves him running south-west (direction 8,
// keypad 1), free a few seconds before the next scene.
void test_aim_behind_while_running(const std::filesystem::path& data) {
    Game game(data, saves_dir());
    game.new_game(0);
    World& w = game.world();
    // The arrival: held in stasis from the first second, running in.
    for (int i = 0; i < 300 && !w.state.avatar_in_stasis; ++i) game.update(kFrameUs);
    for (int i = 0; i < 600 && (w.state.avatar_in_stasis || actor_busy(game.kernel(), 1)); ++i) game.update(kFrameUs);
    CHECK(!w.state.avatar_in_stasis && (w.npc_direction(1) & 0xF) == 8);
    // The weapon drawn (action 7, the fire key's).
    do_anim(game.kernel(), w, game.animations(), 1, 7, kDirCurrent);
    for (int i = 0; i < 120 && actor_busy(game.kernel(), 1); ++i) game.update(kFrameUs);
    CHECK(game.weapon_out());
    // Running on the way he came (keypad 1 with Shift).
    constexpr std::uint8_t kKeypad1 = 0x4F, kDown = 0x50, kShift = 0x2A, kAltKey = 0x38;
    game.key(kShift, false, true, kLeftShift);
    game.key(kKeypad1, false, true, kLeftShift);
    for (int i = 0; i < 30; ++i) game.update(kFrameUs);
    const int run_dir = w.npc_direction(1) & 0xF;
    const std::int32_t x0 = w.get(1)->x, y0 = w.get(1)->y;
    // The aim swung behind him, and the stick's move becomes the step back.
    // Direction 0, one step (0, -1) on the ground, shows as (1, -1/2) on
    // screen: 26.6 degrees.
    game.set_aim_stick(true, 27);
    const std::uint16_t alt = kLeftShift | kLeftAlt | kAlt;
    game.key(kKeypad1, false, false, kLeftShift);
    game.key(kAltKey, false, true, alt);
    game.key(kDown, true, true, alt);
    for (int i = 0; i < 120; ++i) game.update(kFrameUs);
    const std::int32_t dx = w.get(1)->x - x0, dy = w.get(1)->y - y0;
    const int dir = w.npc_direction(1) & 0xF;
    std::printf("aim behind a run (direction %d): faces %d, moved (%d, %d)\n", run_dir, dir, dx, dy);
    CHECK(run_dir == 8);
    CHECK(dir == ((run_dir + 8) & 0xF));
    // Still going the way the stick points, the way he ran.
    CHECK(static_cast<std::int64_t>(dx) * kDirectionDx[run_dir] + static_cast<std::int64_t>(dy) * kDirectionDy[run_dir] > 0);
    game.key(kDown, true, false, alt);
    game.key(kAltKey, false, false, kLeftShift);
    game.key(kShift, false, false, 0);
    game.set_aim_stick(false, 0);
    CHECK(game.machine().faults().empty());
}

// Port addition, the cheat "loot from remains" (death_loot.hpp, GitHub
// issue #2): the humans of the first map's fast area burnt by plasma (fire
// type 6, shape 0x58F). NPCDEATH makes them no body, so BOOTY gives them no
// loot. With the cheat, at the end of each death BOOTY's function 0x20 draws
// the loot of the shape the NPC had alive, into a box that is no body, and
// all of it drops on the floor beside the NPC. What BOOTY draws is random
// (often nothing at this difficulty): the test adds a clip to each box.
void test_remains_loot(const std::filesystem::path& data) {
    for (const bool cheat : {false, true}) {
        Game game(data, saves_dir());
        game.config().remains_loot = cheat;
        game.new_game(0);
        for (int i = 0; i < 300 && game.in_movie(); ++i) game.update(kFrameUs);
        game.update(kFrameUs);
        World& w = game.world();
        CHECK(w.remains_loot() == cheat);

        std::vector<std::uint16_t> called_with;  // the NPC's shape during each call
        std::vector<ObjId> boxed;                // what the boxes held
        WorldHooks hooks = w.hooks();
        hooks.call_class_event = [&w, call = hooks.call_class_event, &called_with, &boxed](
                                     std::string_view name, std::uint16_t event, ObjId npc,
                                     std::span<const std::uint8_t> args) {
            CHECK(name == "BOOTY" && event == 0x20 && args.size() == 2);
            const bool ok = call(name, event, npc, args);
            called_with.push_back(w.get(npc)->shape);
            const auto box = static_cast<ObjId>(args[0] | args[1] << 8);
            if (w.create(0x55F, 0)) w.pop_to_container(box);
            for (ObjId id : w.get(box)->contents) boxed.push_back(id);
            return ok;
        };
        w.set_hooks(hooks);

        std::vector<std::uint16_t> alive;
        std::vector<World::Point> burnt;
        for (ObjId id = 2; id < 256; ++id) {
            const Item* npc = w.get(id);
            if (!npc || npc->off_map || !(npc->flags & kItemFastArea) || w.is_dead(id) ||
                !takes_full_bullet_damage(w, npc->shape))
                continue;
            const std::uint16_t shape = npc->shape;
            apply_damage(game.kernel(), w, game.animations(), id, 1, 0, 250, 6, nullptr);
            npc = w.get(id);
            if (!npc || npc->shape != 0x58F) continue;
            alive.push_back(shape);
            burnt.push_back({npc->x, npc->y, npc->z});
        }
        CHECK(burnt.size() == 2);
        CHECK(w.death_loot().size() == (cheat ? burnt.size() : 0));
        // The loot drops at the end of the death animations.
        for (int i = 0; i < 600; ++i) game.update(kFrameUs);
        std::printf("remains loot (cheat %s): %zu burnt, %zu items dropped\n", cheat ? "on" : "off", burnt.size(),
                    boxed.size());
        CHECK(called_with == (cheat ? alive : std::vector<std::uint16_t>{}));
        CHECK(boxed.size() >= (cheat ? burnt.size() : 0));
        for (ObjId id : boxed) {
            const Item* item = w.get(id);
            bool near = false;
            for (const World::Point& at : burnt)
                near = near || (item && std::abs(item->x - at.x) <= 96 && std::abs(item->y - at.y) <= 96);
            CHECK(item && item->parent == 0 && !(item->flags & kItemEthereal) && near);
        }
        CHECK(w.death_loot().empty());
        CHECK(w.pending_count() == 0);
        CHECK(game.machine().faults().empty());
    }
}

// The hack mover (H, GitHub issue #22 of No Remorse): LOOSECANNON16 allows
// it (DS:84D4) and H (1008:1565) turns it on; the left button then drags the
// item under the cursor (world_mouse.hpp).
void test_hack_mover(const std::filesystem::path& data) {
    Game game(data, saves_dir());
    game.new_game(0);
    World& w = game.world();
    auto run = [&](int frames) {
        for (int i = 0; i < frames; ++i) game.update(kFrameUs);
    };
    for (int i = 0; i < 300 && !w.state.avatar_in_stasis; ++i) run(1);
    for (int i = 0; i < 600 && (w.state.avatar_in_stasis || actor_busy(game.kernel(), 1)); ++i) run(1);
    Surface8 screen(640, 480);
    game.render(screen);
    const auto press = [&](std::uint8_t scancode, std::uint8_t ascii) {
        game.key(scancode, false, true, 0, ascii);
        game.key(scancode, false, false, 0, ascii);
        run(1);
    };
    const GameState::Cheats& c = w.state.cheats;
    press(0x23, 'h');  // no cheats: nothing
    CHECK(!c.hack_mover && !game.message());
    static constexpr std::uint8_t kLooseCannon[] = {0x26, 0x18, 0x18, 0x1F, 0x12, 0x2E, 0x1E,
                                                    0x31, 0x31, 0x18, 0x31, 0x02, 0x07};
    for (const std::uint8_t s : kLooseCannon) press(s, 0);
    CHECK(c.enabled && c.hack_mover_allowed && game.message());
    press(0x1C, 0x0D);  // the message closed
    if (w.state.selection.active) press(0x01, 0x1B);  // started by the S of the code
    press(0x23, 'h');
    CHECK(c.hack_mover && game.message() && game.message()->text() == "HACK MOVER ON");
    press(0x1C, 0x0D);
    CHECK(game.cursor().visible() && game.cursor().frame() == MouseCursor::kArrow);

    // A loose item that falls, drawn on top at a point of the screen.
    const GameState& s = w.state;
    const auto list = build_display_list(w, game.shapes(), View{s.view_left, s.view_top}, s.screen_right + 1,
                                         s.screen_bottom + 1);
    ObjId id = 0;
    int mx = 0, my = 0;
    for (std::size_t i = list.size(); i-- > 0 && id == 0;) {
        const Item& item = *list[i].item;
        const ShapeInfo* info = w.shape_info(item.shape);
        if (item.id < 256 || !info || info->is(ShapeInfo::kFixed) || info->weight == 0 || info->family == 2 ||
            info->family == 9) {
            continue;
        }
        for (int y = std::max(list[i].top, 60); y < std::min(list[i].bottom, 380) && id == 0; ++y) {
            for (int x = std::max(list[i].left, 60); x < std::min(list[i].right, 580) && id == 0; ++x) {
                const auto hits = pick(list, x, y);
                if (!hits.empty() && hits.back() == i) {
                    id = item.id;
                    mx = x;
                    my = y;
                }
            }
        }
    }
    CHECK(id != 0);
    if (id == 0) return;
    const Item* item = w.get(id);
    std::printf("hack mover: item %d shape 0x%X at (%d, %d, %d), mouse (%d, %d)\n", id, item->shape, item->x, item->y,
                item->z, mx, my);
    const int dir = w.npc_direction(1);
    game.mouse_move(mx, my);
    run(1);
    CHECK(game.cursor().x() == mx && game.cursor().y() == my && w.npc_direction(1) == dir);
    game.mouse_button(1, true, mx, my);
    run(MouseQueue::kClickDelay + 1);
    CHECK(game.world_mouse().dragging() && game.world_mouse().held() == id);
    int moves = 0;
    for (const auto [dx, dy] : {std::pair{16, 8}, std::pair{-16, 8}, std::pair{16, -8}, std::pair{-16, -8}}) {
        const std::int32_t x0 = item->x, y0 = item->y, z0 = item->z;
        mx += dx;
        my += dy;
        game.mouse_move(mx, my);
        run(1);
        std::printf("hack mover: mouse (%d, %d), item (%d, %d, %d) -> (%d, %d, %d)\n", mx, my, x0, y0, z0, item->x,
                    item->y, item->z);
        if (item->x == x0 && item->y == y0 && item->z == z0) continue;
        ++moves;
        if (item->z == z0) {
            CHECK(std::abs(item->x - x0 - (dx + 2 * dy)) <= 1 && std::abs(item->y - y0 - (2 * dy - dx)) <= 1);
        }
    }
    CHECK(moves > 0 && w.cells_consistent());
    game.mouse_button(1, false, mx, my);
    run(1);
    CHECK(!game.world_mouse().dragging() && w.pending_count() == 0);
    press(0x23, 'h');
    CHECK(!c.hack_mover && game.message() && game.message()->text() == "HACK MOVER OFF");
    press(0x1C, 0x0D);
    CHECK(!game.cursor().visible());
    CHECK(game.machine().faults().empty());
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

// The load / save screen (13C0:000E, 13C0:0C02): LOAD.BMP / SAVE.BMP with
// their palette (entries 0 and 255 black), slots at (164, 110 + 27 i), the
// selected one outlined in 0x6F, the others in 0xFD.
void test_load_screen(const std::filesystem::path& data) {
    set_regret_data(true);
    const auto flex = FlexArchive::open(data / "STATIC/GUMPS.FLX");
    ShapeCache gumps(flex, ShapeLayout::Flat2D);
    const auto fonts = FontSet::load(data / "STATIC/FONTS.FLX");
    const auto pictures = LoadScreen::Pictures::load(DataFiles(data));
    CHECK(pictures.pages[0] && pictures.pages[1]);
    LoadScreen::Names names{};
    names[1] = "TEST";
    int selected = 1;
    LoadScreen load(gumps, fonts, names, selected, LoadScreen::Mode::kLoad, &pictures);
    const Palette* p = load.palette();
    CHECK(p != nullptr && p == &pictures.pages[1]->palette);
    CHECK(p->colors[0].r == 0 && p->colors[0].b == 0 && p->colors[255].g == 0);
    CHECK(load.slot_rect(0).x0 == 164 && load.slot_rect(0).y0 == 110 && load.slot_rect(2).y0 == 164);
    Surface8 screen(640, 480);
    load.draw(screen);
    shot_surface(screen, *p, "load_screen.png");
    CHECK(screen.row(110)[164] == LoadScreen::kRegretSlotColour);
    CHECK(screen.row(137)[164] == LoadScreen::kRegretSelectedColour);
    LoadScreen save(gumps, fonts, names, selected, LoadScreen::Mode::kSave, &pictures);
    CHECK(save.palette() == &pictures.pages[0]->palette);
    // The proposed save name (--auto-save-names, PortMaster): A on a slot
    // edits the date, every character drawn by the slots' font.
    save.set_auto_name([] { return std::string("12/11/2026 14:32"); });
    save.key(gump_key::kEnter);
    CHECK(save.editing() && save.name() == "12/11/2026 14:32");
    const TextWriter writer(fonts, LoadScreen::kFont);
    for (char c : std::string("0123456789/: ")) CHECK(writer.char_width(static_cast<unsigned char>(c)) > 0);
    save.key(gump_key::kEnter);
    CHECK(save.result() == 1 && save.slot() == 1 && save.name() == "12/11/2026 14:32");
}

// The help (1008:3906, 3B08, 3D42): HELP1.BMP .. HELP5.BMP, no buttons, the
// opening sound 0x1A3; any key but Escape turns the page, 5 back to 0.
void test_help_screen(const std::filesystem::path& data) {
    set_regret_data(true);
    const auto flex = FlexArchive::open(data / "STATIC/GUMPS.FLX");
    ShapeCache gumps(flex, ShapeLayout::Flat2D);
    const auto pictures = HelpScreen::Pictures::load(DataFiles(data));
    for (const auto& page : pictures.pages) CHECK(page && page->picture.width == 640 && page->picture.height == 480);
    std::vector<int> sounds;
    HelpScreen help(gumps, pictures, [](int) { return 0; }, [&sounds](int sfx) { sounds.push_back(sfx); });
    CHECK(help.buttons().empty() && sounds == std::vector<int>{HelpScreen::kRegretOpenSound});
    CHECK(help.page() == 0 && help.palette() == &pictures.pages[0]->palette);
    Surface8 screen(640, 480);
    help.draw(screen);
    shot_surface(screen, *help.palette(), "help_screen.png");
    for (int i = 1; i < 5; ++i) help.key('a');
    CHECK(help.page() == 4 && help.palette() == &pictures.pages[4]->palette);
    help.key(gump_key::kRight);
    CHECK(help.page() == 0 && !help.closed());
    help.key(gump_key::kEscape);
    CHECK(help.closed());
}

}  // namespace

int main(int argc, char** argv) {
    set_languages_dir(CRUSADER_LANGUAGES_DIR);  // the French texts (languages/fr/port.txt)
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
        test_saved_busy_items(data);
        test_keypad_autosave(data);
        test_movie_shows_status_bar(data);
        test_shot_floor_mine(data);
        test_fixture_lights(data);
        test_wall_lamps_and_alarms(data);
        test_regret_lamps(data);
        test_aim_behind_while_running(data);
        test_remains_loot(data);
        test_hack_mover(data);
        test_start_up(data);
        test_load_screen(data);
        test_help_screen(data);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: exception: %s\n", e.what());
        ++failures;
    }
    if (failures != 0) std::fprintf(stderr, "%d failure(s)\n", failures);
    else std::printf("regret_data_tests: ok\n");
    return failures == 0 ? 0 : 1;
}
