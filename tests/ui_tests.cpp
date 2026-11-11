// Gumps and other UI pieces on synthetic shapes (no game data).

#include "core/flex.hpp"
#include "core/shape.hpp"
#include "core/surface.hpp"
#include "engine/ui/enhancements_screen.hpp"
#include "engine/ui/keypad_gump.hpp"
#include "engine/ui/gump_keys.hpp"
#include "engine/ui/xan_player.hpp"
#include "engine/world/render.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
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

void put_u16(std::vector<std::uint8_t>& b, std::size_t at, std::uint16_t v) {
    b[at] = static_cast<std::uint8_t>(v);
    b[at + 1] = static_cast<std::uint8_t>(v >> 8);
}

void put_u24(std::vector<std::uint8_t>& b, std::size_t at, std::uint32_t v) {
    b[at] = static_cast<std::uint8_t>(v);
    b[at + 1] = static_cast<std::uint8_t>(v >> 8);
    b[at + 2] = static_cast<std::uint8_t>(v >> 16);
}

void put_u32(std::vector<std::uint8_t>& b, std::size_t at, std::uint32_t v) {
    for (int k = 0; k < 4; ++k) b[at + static_cast<std::size_t>(k)] = static_cast<std::uint8_t>(v >> (8 * k));
}

// A Flat2D (GUMPS.FLX) frame: a solid block of one palette index.
std::vector<std::uint8_t> solid_frame(int width, int height, std::uint8_t color) {
    std::vector<std::uint8_t> f(20 + 4 * static_cast<std::size_t>(height), 0);
    put_u32(f, 0, 0);  // uncompressed
    put_u32(f, 4, static_cast<std::uint32_t>(width));
    put_u32(f, 8, static_cast<std::uint32_t>(height));
    put_u32(f, 12, 0);  // xoff
    put_u32(f, 16, 0);  // yoff
    // Line offsets are relative to their own position; all lines share one
    // RLE stream: skip 0, run `width`, then the colour bytes.
    for (int y = 0; y < height; ++y) put_u32(f, 20 + 4 * static_cast<std::size_t>(y), 4u * (height - y));
    f.push_back(0);
    f.push_back(static_cast<std::uint8_t>(width));
    f.insert(f.end(), static_cast<std::size_t>(width), color);
    return f;
}

// A Flat2D shape blob holding `frames`.
std::vector<std::uint8_t> shape_blob(const std::vector<std::vector<std::uint8_t>>& frames) {
    const std::size_t table = 6 + 8 * frames.size();
    std::vector<std::uint8_t> s(table, 0);
    put_u16(s, 4, static_cast<std::uint16_t>(frames.size()));
    std::size_t offset = table;
    for (std::size_t i = 0; i < frames.size(); ++i) {
        put_u24(s, 6 + 8 * i, static_cast<std::uint32_t>(offset));
        put_u32(s, 10 + 8 * i, static_cast<std::uint32_t>(frames[i].size()));
        s.insert(s.end(), frames[i].begin(), frames[i].end());
        offset += frames[i].size();
    }
    return s;
}

// KeypadGump::draw (13C8:08F5, GUMPSHAP_1340_0242): the frame rect is
// 0, 0, width - 1, height - 1, so the next digit starts at the frame's width
// + 1, not + 2. Frames 4 and 5 pixels wide show the one-pixel difference.
void test_keypad_digit_spacing() {
    const std::vector<std::vector<std::uint8_t>> digit_frames = {solid_frame(4, 2, 0x21),
                                                                 solid_frame(5, 2, 0x22)};
    const auto blob = shape_blob(digit_frames);
    const auto path = std::filesystem::temp_directory_path() / "crusader_keypad_spacing.flx";
    {
        FlexWriter out(13);  // GUMPS shapes 0..12; only the digits (12) are used here
        out.add(KeypadGump::kDigitShape, blob);
        out.write(path);
    }
    const FlexArchive flex = FlexArchive::open(path);
    ShapeCache gumps(flex, ShapeLayout::Flat2D);

    KeypadGump pad(gumps, 0, [](int) {}, 640, 480);
    pad.key('1');
    pad.key('2');
    CHECK(pad.digit_count() == 2);

    Surface8 screen(640, 480, 0);
    pad.draw(screen);
    int first = -1, second = -1;
    for (int y = 0; y < screen.height && (first < 0 || second < 0); ++y) {
        for (int x = 0; x < screen.width; ++x) {
            const std::uint8_t c = screen.row(y)[x];
            if (c == 0x21 && first < 0) first = x;
            if (c == 0x22 && second < 0) second = x;
        }
    }
    CHECK(first >= 0 && second >= 0);
    CHECK(second - first == 4 + 1);
}

void test_vibration_settings() {
    FontSet fonts;
    GameConfig config;
    int pad_focus = 0;
    int previews = 0;
    EnhancementsScreen screen(fonts, config, pad_focus, [&] { ++previews; });

    for (int i = 0; i < EnhancementsScreen::kVibrationIntensityRow; ++i) screen.key(gump_key::kDown);
    screen.key(gump_key::kLeft);
    CHECK(config.vibration_intensity == 90 && previews == 1);
    screen.key(gump_key::kEnter);
    CHECK(config.vibration_intensity == 100 && previews == 2);
    screen.key(gump_key::kRight);
    CHECK(config.vibration_intensity == 100 && previews == 2);

    EnhancementsScreen toggle(fonts, config, pad_focus);
    for (int i = 0; i < EnhancementsScreen::kVibrationRow; ++i) toggle.key(gump_key::kDown);
    toggle.key(gump_key::kEnter);
    CHECK(!config.vibration);
}

// A RIFF chunk: id, size, data, padding.
void put_chunk(std::vector<std::uint8_t>& out, const char* id, const std::vector<std::uint8_t>& data) {
    out.insert(out.end(), id, id + 4);
    const auto n = static_cast<std::uint32_t>(data.size());
    for (int k = 0; k < 4; ++k) out.push_back(static_cast<std::uint8_t>(n >> (8 * k)));
    out.insert(out.end(), data.begin(), data.end());
    if (n & 1) out.push_back(0);
}

std::vector<std::uint8_t> list(const char* type, const std::vector<std::uint8_t>& body) {
    std::vector<std::uint8_t> d(type, type + 4);
    d.insert(d.end(), body.begin(), body.end());
    return d;
}

std::vector<std::uint8_t> u32s(std::initializer_list<std::uint32_t> v) {
    std::vector<std::uint8_t> d;
    for (std::uint32_t x : v)
        for (int k = 0; k < 4; ++k) d.push_back(static_cast<std::uint8_t>(x >> (8 * k)));
    return d;
}

// A 4 x 4 Xxan movie, one black frame (all luma and chroma 0): table entry 0
// is 0xFF, which 1448:03B4 turns into the colour nearest to colour 255.
void test_regret_movie() {
    CHECK(regret_movie_base_name("1a") == "01A" && regret_movie_base_name("anim02") == "ANIM02");

    std::vector<std::uint8_t> pal(768, 0);
    for (int k = 0; k < 3; ++k) {
        pal[3 * 7 + k] = 0xF0;    // 60 in 6 bits
        pal[3 * 255 + k] = 0xFC;  // 63
    }
    std::vector<std::uint8_t> move, strl, hdrl, movi, riff;
    put_chunk(move, "_PC_", u32s({4, 1}));
    put_chunk(move, "PAL8", pal);
    put_chunk(move, "BEST", {0xE0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC});
    // A second pair (from frame 1 on, "SHOT"): never used.
    put_chunk(move, "PAL8", std::vector<std::uint8_t>(768, 0x80));
    put_chunk(move, "BEST", {0xE0, 0x01, 0x01, 0x01, 0x01, 0xFC});
    put_chunk(move, "SHOT", u32s({0, 0, 1, 1}));
    std::vector<std::uint8_t> strh = {'v', 'i', 'd', 's', 'X', 'x', 'a', 'n'};
    strh.resize(56);
    put_chunk(strl, "strh", strh);
    put_chunk(strl, "strf", u32s({40, 4, 4, 0x00100001, 0}));
    put_chunk(strl, "LIST", list("MOVE", move));
    put_chunk(hdrl, "LIST", list("strl", strl));
    // Type 0, no chroma, no corrections; 8 luma deltas of 0 ("00" each), end ("1").
    std::vector<std::uint8_t> frame = u32s({0, 0, 0});
    frame.insert(frame.end(), {2, 2, 0, 1, 3, 2, 0x00, 0x00, 0x80});
    put_chunk(movi, "00db", frame);
    std::vector<std::uint8_t> body = list("AVI ", {});
    put_chunk(body, "LIST", list("hdrl", hdrl));
    put_chunk(body, "LIST", list("movi", movi));
    put_chunk(riff, "RIFF", body);

    XanMovie movie = XanMovie::parse(riff);
    CHECK(movie.width() == 4 && movie.height() == 4 && movie.version() == 4 && movie.chunks().size() == 1);
    FontSet fonts;
    XanPlayer player(std::move(movie), {}, false, false, true, fonts);
    CHECK(player.darkest() == 0);
    CHECK(player.screen().row(0)[0] == 0);
    const std::array<std::uint8_t, 768> vga = player.palette().to_vga();
    CHECK(vga[3 * 255] == 63 && vga[3 * 7] == 60);
    player.advance(0, false);
    CHECK(player.frame() == 1 && !player.finished());
    CHECK(player.screen().row(238)[318] == 7 && player.screen().row(241)[321] == 7);
    player.advance(1'000'000 / 15, false);
    CHECK(player.finished());
}

}  // namespace

int main() {
    test_regret_movie();
    test_keypad_digit_spacing();
    test_vibration_settings();
    if (failures == 0) std::printf("all ui checks passed\n");
    return failures == 0 ? 0 : 1;
}
