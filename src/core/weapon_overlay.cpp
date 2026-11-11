#include "core/weapon_overlay.hpp"

#include "core/dtable.hpp"
#include "core/flex.hpp"

namespace crusader {

// The original reads the entries of the player's shape when it changes
// (1178:0000, through FastFlex); the port reads them all at once.
// @orig 1178:0000
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

// @orig 1178:0175 1178:01BA
std::optional<int> WeaponOverlay::weapon_class(std::uint16_t weapon_shape) {
    const int c = dtable::overlay_class(weapon_shape);
    return c < 0 ? std::nullopt : std::optional<int>(c);
}

std::uint16_t WeaponOverlay::class_shape(int weapon_class) { return dtable::overlay_class_shape(weapon_class); }

// @orig 1178:0246
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
