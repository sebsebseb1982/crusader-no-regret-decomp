// Checks the format readers against the original game data (data/, not in the
// repository): runs locally only.
// Usage: core_data_tests <data_dir>   (skips with exit code 0 when data is absent)

#include "core/anim_dat.hpp"
#include "core/flex.hpp"
#include "core/font.hpp"
#include "core/palette.hpp"
#include "core/shape.hpp"
#include "core/xform.hpp"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <set>
#include <utility>

using namespace crusader;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

// Decodes every frame of every shape in an archive into a scratch surface:
// any out-of-range read in the RLE stream throws.
// Frames whose RLE overshoots the frame width in the original data (line 0).
const std::set<std::pair<std::size_t, std::size_t>> kKnownOvershoot = {{1431, 131}, {1431, 182}};

void decode_all(const std::filesystem::path& path, ShapeLayout layout) {
    const auto flex = FlexArchive::open(path);
    const bool is_shapes = path.filename() == "SHAPES.FLX";
    std::size_t shapes = 0, frames = 0, errors = 0, mismatched = 0;
    for (std::size_t i = 0; i < flex.count(); ++i) {
        const auto entry = flex.entry(i);
        if (entry.empty()) continue;
        try {
            const auto shape = Shape::parse(entry, layout);
            for (std::size_t k = 0; k < shape.frames().size(); ++k) {
                const auto& f = shape.frames()[k];
                Surface8 s(f.width + 2, f.height + 2);
                f.draw(s, f.xoff + 1, f.yoff + 1);
                f.draw(s, f.xoff + 1, f.yoff + 1, true);
                const bool known = is_shapes && kKnownOvershoot.count({i, k});
                if (!f.runs_match_width() && !known) {
                    std::fprintf(stderr, "  %s #%zu frame %zu: runs != width\n",
                                 path.filename().string().c_str(), i, k);
                    ++mismatched;
                }
                ++frames;
            }
            ++shapes;
        } catch (const std::exception& e) {
            if (errors++ < 5) std::fprintf(stderr, "  %s #%zu: %s\n", path.filename().string().c_str(), i, e.what());
        }
    }
    std::printf("%s: %zu shapes, %zu frames decoded, %zu errors, %zu frames with runs != width\n",
                path.filename().string().c_str(), shapes, frames, errors, mismatched);
    check(errors == 0, "all shapes decode");
    check(mismatched == 0, "every RLE line ends exactly at the frame width");
}

void check_anim_dat(const std::filesystem::path& data) {
    // Synthetic frame: frame 0x234 mirrored, z -8, sfx 5, distance -2, ground check.
    const std::uint8_t raw[8] = {0x34, 0x82, 0xF8, 0x05, 0xFE, 0x03, 0x00, 0x00};
    const AnimFrame f = AnimFrame::decode(std::span<const std::uint8_t, 8>(raw));
    check(f.frame == 0x234 && f.mirrored && !f.special && !f.fires_weapon() && f.delta_z == -8 && f.sfx == 5, "frame fields");
    check(f.distance == -2 && f.ground_check, "9-bit signed distance and ground flag");

    const auto anims = AnimDat::load(data / "STATIC/ANIM.DAT");
    const auto shapes = FlexArchive::open(data / "STATIC/SHAPES.FLX");
    std::size_t animated = 0, frames = 0, bad = 0;
    for (std::uint16_t s = 0; s < AnimDat::kShapes; ++s) {
        if (!anims.has_shape(s)) continue;
        ++animated;
        const auto shape = Shape::parse(shapes.entry(s), ShapeLayout::World);
        for (std::uint16_t a = 0; a < AnimDat::kActions; ++a) {
            const AnimAction* act = anims.action(s, a);
            if (!act) continue;
            for (const AnimFrame& fr : act->frames) {
                ++frames;
                bad += fr.frame >= shape.frames().size();
            }
        }
    }
    std::printf("ANIM.DAT: %zu animated shapes, %zu actions, %zu frames, %zu referencing missing shape frames\n",
                animated, anims.action_count(), frames, bad);
    check(animated == 41 && anims.action_count() == 357 && frames == 22056, "ANIM.DAT layout");
    check(bad == 0, "every animation frame exists in its shape");

    // Player (shape 1), action 1: 17 frames, 8 directions; frame 1 facing 0 moves by 8.
    const AnimAction* walk = anims.action(1, 1);
    check(walk && walk->frame_count() == 17 && !walk->sixteen_directions() && walk->looping(), "player action 1");
    check(walk && walk->frame(0, 1)->distance == 8 && walk->frame(1, 1) == walk->frame(0, 1), "8-direction lookup");

    // The player's shot (action 8): the weapon fires on frame 0 only.
    const AnimAction* shot = anims.action(1, 8);
    check(shot && shot->frame(0, 0)->fires_weapon() && !shot->frame(0, 1)->fires_weapon() &&
              !shot->frame(0, 2)->fires_weapon(),
          "muzzle offsets mark the firing frame");
}

void check_xform(const std::filesystem::path& data) {
    const auto x = XformTable::load(data / "STATIC/XFORMPAL.DAT");
    int translucent = 0;
    for (int c = 0; c < 256; ++c) translucent += x.translucent(static_cast<std::uint8_t>(c));
    check(translucent == 7 && x.translucent(8) && x.translucent(14) && !x.translucent(15), "colours 8..14 are translucent");
    check(x.blend(0x20, 0x55) == 0x20, "opaque colour replaces the background");

    // The status bar boxes (GUMPS.FLX shape 1) are drawn with the translucent blitter.
    const auto gumps = FlexArchive::open(data / "STATIC/GUMPS.FLX");
    const auto box = Shape::parse(gumps.entry(1), ShapeLayout::Flat2D);
    int seen = 0;
    const auto& f = box.frames()[0];
    for (int y = 0; y < f.height; ++y)
        for (int fx = 0; fx < f.width; ++fx) seen += f.pixel(fx, y) >= 0 && x.translucent(static_cast<std::uint8_t>(f.pixel(fx, y)));
    check(seen > 0, "status box background uses translucent colours");
}

void check_fonts(const std::filesystem::path& data) {
    const auto fonts = FontSet::load(data / "STATIC/FONTS.FLX");
    std::size_t glyphs = 0, bad_runs = 0;
    for (int font = 0; font < FontSet::kMaxFonts; ++font) {
        for (int c = 0; c < 256; ++c) {
            const ShapeFrame* g = fonts.glyph(font, static_cast<unsigned char>(c));
            if (!g || g->width == 0) continue;
            ++glyphs;
            Surface8 s(g->width + 2, g->height + 2);
            g->draw(s, g->xoff + 1, g->yoff + 1);
            bad_runs += !g->runs_match_width();
        }
    }
    std::printf("FONTS.FLX: %zu glyphs, %zu unreadable frames\n", glyphs, fonts.bad_frames());
    check(bad_runs == 0, "every glyph decodes to its width");
    check(fonts.bad_frames() <= 128 && fonts.glyph(2, 127) && fonts.glyph(2, 127)->width > 0,
          "only font 2 frames 128..255 are unreadable");
    TextWriter t(fonts, 0);
    check(t.char_width('A') == 7 && t.char_height('A') == 14 && t.baseline() == 12, "font 0 metrics (+1 spacing)");
    TextWriter t1(fonts, 1);
    check(t1.char_width('A') == 5 && t1.char_height('A') == 9, "font 1 metrics (no spacing)");
    check(TextWriter(fonts, 40).font() == 0, "font numbers above 31 become 0");

    const auto one = t.measure("AA");
    check(one.width == 14 && one.height == 14, "one-line measure");
    const auto two = t.measure("A\nAAA");
    check(two.width == t.char_width('\n') + 21 && two.height == 28, "'\\n' advance counts in the next line");

    Surface8 s(200, 40, 0xFF);
    t.print(s, 10, 20, "Ab\nc");
    check(t.x() == t.char_width('c') &&t.y() == 20 + t.char_height('\n'),
          "newline returns to the viewport edge");
    int ink = 0;
    for (auto p : s.pixels) ink += p != 0xFF;
    check(ink > 0, "text drawn");
    check(t.print_clipped(s, 0, 0, 20, "AAAA") == 2, "clipped print stops at the width");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const std::filesystem::path data = argc > 1 ? argv[1] : "data";
    if (!std::filesystem::exists(data / "STATIC/SHAPES.FLX")) {
        std::printf("game data not found in %s, skipping\n", data.string().c_str());
        return 0;
    }
    try {
        const auto flex = FlexArchive::open(data / "USECODE/EUSECODE.FLX");
        check(flex.count() == 3074, "EUSECODE.FLX has 3074 entries");
        check(flex.entry(0).empty(), "entry 0 is empty");

        const auto pal = Palette::load(data / "STATIC/GAMEPAL.PAL");
        check(Palette::from_vga(pal.to_vga()) == pal, "6-bit / 8-bit palette round trip");
        check(pal.colors[0].r == 0 && pal.colors[0].g == 0 && pal.colors[0].b == 0, "palette entry 0 is black");

        decode_all(data / "STATIC/SHAPES.FLX", ShapeLayout::World);
        decode_all(data / "STATIC/GUMPS.FLX", ShapeLayout::Flat2D);
        check_xform(data);
        check_fonts(data);

        check_anim_dat(data);

        const auto mouse = Shape::parse(read_file(data / "STATIC/MOUSE.SHP"), ShapeLayout::Flat2D);
        check(!mouse.frames().empty(), "MOUSE.SHP decodes");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        ++failures;
    }
    std::printf(failures ? "%d failure(s)\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}