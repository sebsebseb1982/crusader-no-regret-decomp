#pragma once
// 256-colour palette (GRAPH\PALETTE.C / BASEPAL.C). Files store 6-bit VGA DAC
// components (0..63); they are expanded to 8 bits on load.

#include <array>
#include <cstdint>
#include <filesystem>

namespace crusader {

struct Rgb {
    std::uint8_t r, g, b;
};

// 6-bit VGA component to 8 bits (c << 2 | c >> 4) and back (e >> 2, exact).
constexpr std::uint8_t expand6(std::uint8_t c) { return static_cast<std::uint8_t>(c << 2 | c >> 4); }
constexpr std::uint8_t reduce8(std::uint8_t e) { return static_cast<std::uint8_t>(e >> 2); }

struct Palette {
    std::array<Rgb, 256> colors{};

    // .PAL file: 256 * { r, g, b } with 6-bit components.
    static Palette load(const std::filesystem::path& path);

    // palette_new_default: every entry the same 6-bit colour.
    static Palette filled(std::uint8_t r6, std::uint8_t g6, std::uint8_t b6);

    // The 768 DAC components (6 bits), in r, g, b order per entry.
    std::array<std::uint8_t, 768> to_vga() const;
    static Palette from_vga(const std::array<std::uint8_t, 768>& vga);

    // @orig none: comparison for the tests
    bool operator==(const Palette& o) const {
        for (int i = 0; i < 256; ++i) {
            const Rgb& a = colors[i];
            const Rgb& b = o.colors[i];
            if (a.r != b.r || a.g != b.g || a.b != b.b) return false;
        }
        return true;
    }
};

constexpr std::uint8_t kTransparentIndex = 0xFF;  // vport_blit_transparent

}  // namespace crusader
