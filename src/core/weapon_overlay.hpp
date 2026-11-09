#pragma once
// Weapon drawn over the player (STATIC/WPNOVLAY.DAT, WPNCACHE.C segment 1160;
// drawn by DLIST.C 11A0:05xx). See docs/combat.md, "Arme superposée".
//
// Flex of 64 entries, one per action of the player's shape (1). An entry
// holds 15 blocks, one per weapon class; a block has, for each direction
// then each frame of the action, 4 bytes: x offset (s8), y offset (s8), frame
// of the class's overlay shape (u16). Only actions with frames and header
// byte 1 bit 1 are used; 8-direction actions halve the direction.
// Weapon class (DS:2B82, by weapon shape) and overlay shape (DS:2B64):
//   814, 815 -> 0 (0x1A1)   816 -> 1 (0x36D)   908, 819, 820, 906 -> 2 (0x36F)
//   910, 904, 909, 907, 902 -> 3 (0x36E)   818 -> 4 (0x33B)

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

    // WPNCACHE_1160_0572 / 05B7: class of a weapon shape (nullopt if none).
    static std::optional<int> weapon_class(std::uint16_t weapon_shape);
    // WPNCACHE_1160_0771: overlay shape of a class.
    static std::uint16_t class_shape(int weapon_class);

    // WPNCACHE_1160_0738: the action has overlay data.
    bool has(std::uint16_t action) const { return action < entries_.size() && !entries_[action].empty(); }

    // WPNCACHE_1160_0643: the piece shown for `weapon_shape` during frame
    // `index` of `action` (as described by `header`) facing `dir` (0..15).
    std::optional<Piece> piece(std::uint16_t weapon_shape, std::uint16_t action, const AnimAction& header, int dir,
                               int index) const;

private:
    std::vector<std::vector<std::uint8_t>> entries_;
};

}  // namespace crusader
