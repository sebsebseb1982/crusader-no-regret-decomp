#pragma once
// Weapon drawn over the player (STATIC/WPNOVLAY.DAT, WPNCACHE.C segment 1178,
// No Remorse 1160: the same code, other tables; drawn by DLIST.C). See
// docs/combat.md, "Arme superposée".
//
// Flex of 64 entries, one per action of the player's shape (1). An entry
// holds 15 blocks, one per weapon class; a block has, for each direction
// then each frame of the action, 4 bytes: x offset (s8), y offset (s8), frame
// of the class's overlay shape (u16). Only actions with frames and header
// byte 1 bit 1 are used; 8-direction actions halve the direction.
// Weapon class (dtable::overlay_class, by weapon shape) and overlay shape:
//   815 -> 0 (0x1A1)   816 -> 1 (0x36D)   908, 819, 820, 906, 1525, 1526 -> 2
//   (0x36F)   910, 904, 909, 907, 902 -> 3 (0x36E)   818, 408 -> 4 (0x33B)
// (No Remorse: 814 in class 0 instead of 408; neither 1525 nor 1526).

#include "core/anim_dat.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace crusader {

class WeaponOverlay {
public:
    struct Piece {
        std::uint16_t shape;
        std::uint16_t frame;
        int dx, dy;
    };

    static WeaponOverlay load(const std::filesystem::path& path);

    // 1178:0175 / 01BA: class of a weapon shape (nullopt if none).
    static std::optional<int> weapon_class(std::uint16_t weapon_shape);
    // 1178:0374: overlay shape of a class.
    static std::uint16_t class_shape(int weapon_class);

    // 1178:033B: the action has overlay data.
    bool has(std::uint16_t action) const { return action < entries_.size() && !entries_[action].empty(); }

    // 1178:0246: the piece shown for `weapon_shape` during frame
    // `index` of `action` (as described by `header`) facing `dir` (0..15).
    std::optional<Piece> piece(std::uint16_t weapon_shape, std::uint16_t action, const AnimAction& header, int dir,
                               int index) const;

private:
    std::vector<std::vector<std::uint8_t>> entries_;
};

}  // namespace crusader
