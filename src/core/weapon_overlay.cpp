#include "core/weapon_overlay.hpp"

#include "core/flex.hpp"

#include <array>

namespace crusader {

namespace {

// DS:2B82: weapon shape, class.
struct WeaponClass {
    std::uint16_t shape;
    int weapon_class;
};
constexpr std::array<WeaponClass, 13> kClasses = {{
    {0x32E, 0}, {0x32F, 0}, {0x330, 1}, {0x38C, 2}, {0x332, 4}, {0x333, 2}, {0x334, 2},
    {0x38E, 3}, {0x388, 3}, {0x38A, 2}, {0x38D, 3}, {0x38B, 3}, {0x386, 3},
}};
// DS:2B64.
constexpr std::array<std::uint16_t, 5> kClassShapes = {0x1A1, 0x36D, 0x36F, 0x36E, 0x33B};

}  // namespace

// @orig 1160:03FF
WeaponOverlay WeaponOverlay::load(const std::filesystem::path& path) {
    const FlexArchive flex = FlexArchive::open(path);
    WeaponOverlay o;
    o.entries_.resize(flex.count());
    for (std::size_t i = 0; i < flex.count(); ++i) {
        const auto e = flex.entry(i);
        o.entries_[i].assign(e.begin(), e.end());
    }
    return o;
}

// @orig 1160:0572 1160:05B7
std::optional<int> WeaponOverlay::weapon_class(std::uint16_t weapon_shape) {
    for (const WeaponClass& w : kClasses) {
        if (w.shape == weapon_shape) return w.weapon_class;
    }
    return std::nullopt;
}

std::uint16_t WeaponOverlay::class_shape(int weapon_class) {
    return weapon_class >= 0 && weapon_class < static_cast<int>(kClassShapes.size()) ? kClassShapes[weapon_class] : 0;
}

// @orig 1160:0643
std::optional<WeaponOverlay::Piece> WeaponOverlay::piece(std::uint16_t weapon_shape, std::uint16_t action,
                                                         const AnimAction& header, int dir, int index) const {
    const auto c = weapon_class(weapon_shape);
    if (!c || !has(action) || header.frame_count() == 0 || !(header.flags() & 0x02)) return std::nullopt;
    const std::size_t frames = header.frame_count();
    const std::size_t dirs = header.directions();
    if (!header.sixteen_directions()) dir >>= 1;
    const std::size_t block = frames * dirs * 4;
    const std::size_t at = block * static_cast<std::size_t>(*c) +
                           (static_cast<std::size_t>(dir & 0xF) * frames + static_cast<std::size_t>(index)) * 4;
    const auto& e = entries_[action];
    if (at + 4 > e.size()) return std::nullopt;
    const std::uint16_t frame = static_cast<std::uint16_t>(e[at + 2] | e[at + 3] << 8);
    return Piece{class_shape(*c), frame, static_cast<std::int8_t>(e[at]), static_cast<std::int8_t>(e[at + 1])};
}

}  // namespace crusader
