#include "core/palette.hpp"

#include "core/flex.hpp"

#include <stdexcept>

namespace crusader {

// @orig 1268:03A3
Palette Palette::load(const std::filesystem::path& path) {
    const auto data = read_file(path);
    if (data.size() < 256 * 3) {
        throw std::runtime_error(path.string() + ": palette must hold 768 bytes");
    }
    std::array<std::uint8_t, 768> vga{};
    for (std::size_t i = 0; i < vga.size(); ++i) vga[i] = data[i] & 0x3F;
    return from_vga(vga);
}

Palette Palette::filled(std::uint8_t r6, std::uint8_t g6, std::uint8_t b6) {
    Palette pal;
    pal.colors.fill({expand6(r6 & 0x3F), expand6(g6 & 0x3F), expand6(b6 & 0x3F)});
    return pal;
}

// @orig none: format conversion
std::array<std::uint8_t, 768> Palette::to_vga() const {
    std::array<std::uint8_t, 768> vga{};
    for (std::size_t i = 0; i < 256; ++i) {
        vga[3 * i] = reduce8(colors[i].r);
        vga[3 * i + 1] = reduce8(colors[i].g);
        vga[3 * i + 2] = reduce8(colors[i].b);
    }
    return vga;
}

// @orig none: format conversion
Palette Palette::from_vga(const std::array<std::uint8_t, 768>& vga) {
    Palette pal;
    for (std::size_t i = 0; i < 256; ++i) {
        pal.colors[i] = {expand6(vga[3 * i]), expand6(vga[3 * i + 1]), expand6(vga[3 * i + 2])};
    }
    return pal;
}

}  // namespace crusader
