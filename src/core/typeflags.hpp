#pragma once
// Per-shape properties (STATIC/TYPEFLAG.DAT, read by TYPE.C): 9 bytes per shape.
//
//   byte 0  flags: 1 fixed, 2 solid, 4 sea, 8 land, 0x10 occludes, 0x20 bag,
//                  0x40 damaging, 0x80 noisy
//   byte 1  flags: 1 draw, 2 ignore, 4 roof, 8 translucent; bits 4-7 family (low)
//   byte 2  bit 0 family (bit 4); bits 1-4 equip type; bits 5-7 footpad x (low)
//   byte 3  bits 0-1 footpad x (high); bits 2-6 footpad y; bit 7 footpad z (low)
//   byte 4  bits 0-3 footpad z (high); bits 4-7 animation type
//   byte 5  bits 0-3 animation data; bits 4-7 animation speed
//   byte 6  flags: 1 editor, 2 selectable, 4 preload, 8 sound, 0x10 targetable,
//                  0x20 NPC, 0x40 ?, 0x80 ?
//   byte 7  weight      byte 8  volume
// Footpad dimensions are 5-bit values in world tiles.

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace crusader {

struct ShapeInfo {
    enum Flag : std::uint32_t {
        kFixed = 1u << 0, kSolid = 1u << 1, kSea = 1u << 2, kLand = 1u << 3,
        kOccludes = 1u << 4, kBag = 1u << 5, kDamaging = 1u << 6, kNoisy = 1u << 7,
        kDraw = 1u << 8, kIgnore = 1u << 9, kRoof = 1u << 10, kTranslucent = 1u << 11,
        kEditor = 1u << 16, kSelectable = 1u << 17, kPreload = 1u << 18, kSound = 1u << 19,
        kTargetable = 1u << 20, kNpc = 1u << 21, kUnknown66 = 1u << 22, kUnknown67 = 1u << 23,
    };

    std::uint32_t flags = 0;
    std::uint8_t family = 0;
    std::uint8_t equip_type = 0;
    std::uint8_t footpad_x = 0;
    std::uint8_t footpad_y = 0;
    std::uint8_t footpad_z = 0;
    std::uint8_t anim_type = 0;
    std::uint8_t anim_data = 0;
    std::uint8_t anim_speed = 0;
    std::uint8_t weight = 0;
    std::uint8_t volume = 0;
    // The 9 bytes as read, for Item::I_getTypeFlag (any bit by number).
    std::array<std::uint8_t, 9> bytes{};

    bool is(Flag f) const { return flags & f; }

    static ShapeInfo decode(const std::uint8_t* d);
};

std::vector<ShapeInfo> load_typeflags(const std::filesystem::path& path);

}  // namespace crusader
