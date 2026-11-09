#pragma once
// The screen sprite manager (SPMGR.C, segment 11A8): 64 slots of 19 bytes
// (DS:2D87) for shapes drawn over the scene after it, at a world position
// (x, y, z) turned into a screen place like any item. SpriteMgr_add
// (11A8:0115) takes the first free slot (-1 when all are used), SpriteMgr_move
// (051B) changes the position, 030F the frame, SpriteMgr_remove (01C5) frees
// the slot.
//
// The port keeps here the sprites of the aiming mode (aim_mode.hpp); the
// impacts and explosions of SpriteProcess, also screen sprites in the
// original, are world items flagged sprite_only, and the frame of the item
// selection is drawn directly by the game view. Not reproduced: the
// translucency flag (0x1000), the redraw of the screen parts a sprite left
// (11A8:071E), the saved state (save game entries 30 to 32).

#include <array>
#include <cstdint>

namespace crusader {

struct ScreenSprite {
    std::uint16_t shape = 0;  // 0: free
    std::uint16_t frame = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint8_t z = 0;
};

class ScreenSprites {
public:
    static constexpr int kSlots = 64;

    // @orig 11A8:0115
    int add(std::uint16_t shape, std::uint16_t frame, std::int32_t x, std::int32_t y, std::uint8_t z) {
        for (int i = 0; i < kSlots; ++i) {
            if (slots_[static_cast<std::size_t>(i)].shape != 0) continue;
            slots_[static_cast<std::size_t>(i)] = ScreenSprite{shape, frame, x, y, z};
            return i;
        }
        return -1;
    }
    void remove(int slot) {
        if (ScreenSprite* s = at(slot)) *s = ScreenSprite{};
    }
    // @orig 11A8:071E
    void move(int slot, std::int32_t x, std::int32_t y, std::uint8_t z) {
        if (ScreenSprite* s = at(slot)) {
            s->x = x;
            s->y = y;
            s->z = z;
        }
    }
    void set_frame(int slot, std::uint16_t frame) {
        if (ScreenSprite* s = at(slot)) s->frame = frame;
    }
    const ScreenSprite* get(int slot) const {
        return slot >= 0 && slot < kSlots && slots_[static_cast<std::size_t>(slot)].shape != 0
                   ? &slots_[static_cast<std::size_t>(slot)]
                   : nullptr;
    }
    const std::array<ScreenSprite, kSlots>& all() const { return slots_; }

private:
    ScreenSprite* at(int slot) {
        return slot >= 0 && slot < kSlots ? &slots_[static_cast<std::size_t>(slot)] : nullptr;
    }

    std::array<ScreenSprite, kSlots> slots_{};
};

}  // namespace crusader
