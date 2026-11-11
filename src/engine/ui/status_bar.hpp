#pragma once
// The status bar at the bottom of the screen (segment 11C8, "CruStatusGump" in
// ScummVM): five boxes for the weapon, its ammunition, the active
// inventory item, health and energy.
//
// Layout (11C8:0222): box i is GUMPS.FLX shape 1, frame {0, 1, 0, 2, 3}[i]
// (table DS:36AB), its top-left corner at x = 15 + 127 * i, y = 450. The
// spacing is meant to be (640 - box width) / 5, but the width is read before
// the box rectangle is filled in, while it is still all zeros: it is always
// (640 - 1) / 5 = 127, and the margin 127 / 8 = 15.
//
// Drawing (11C8:03A3), per box, over the world view:
// - background, opaque (No Remorse: translucent): boxes 0 and 1 only while the
//   player (NPC 1) is the controlled NPC and has a weapon (box 1: a weapon
//   that takes ammunition), boxes 2 and 4 while the player is controlled, box
//   3 always;
// - nothing more unless the player is controlled, except for box 3;
// - box 0: the weapon icon (GUMPS.FLX shape 3) at the centre of the box, and
//   the weapon's name in font 18, its right end 4 pixels left of the box's
//   right edge, at y + 0x14 (new in No Regret);
// - box 1 (font 8; No Remorse: font 15, (+30, +11) and (+75, +11)): the
//   weapon's quality (rounds loaded) at (+38, +8), and at (+90, +8) the
//   quality of the first item of the player's equipment that is that
//   ammunition (NPC_10f8_40b9), or "0";
// - box 2: the item icon 0x1C pixels left of the centre (shape 3 frame 13 for
//   the credits 0x112, shape 4 for family 12, shape 5 otherwise; icons 3, 5
//   and 7 of shape 5 become 18, 17 and 19 when the item's frame is not 0); its
//   quality in font 12 when above 1, its name in font 18 and, for an item of
//   frame 1, "MESSAGE" (shape 0x4D4) or "ACTIVE", all right-aligned like the
//   weapon's name at y + 0x14, + 4 and + 0xA (No Remorse: the icon centred,
//   the quality at (+75, +18), no text);
// - box 3: a health bar of colour 0x41, (+0x23, +8) to (+0x23 + w, +0x13)
//   inclusive, w = (uint16)(hp << 6) / max hp, max hp from DTABLE.FLX, if hp
//   is not 0 and the NPC is not frozen (shape 0x5D6) (No Remorse: (+0x22, +7)
//   to (+0x22 + w, +0x14), hp * 66);
// - box 4: an energy bar of colour 0xF5, same place, w = energy * 64 / 10000.
// Weapon and item come from the player's record (DS:898C holds the player's
// id; it is never written by name, and NPC_10f0_395e reads NPC 1's equipment
// directly), health and energy from the controlled NPC.
//
// The original redraws only the boxes under the frame's dirty rectangles and
// saves the world view under each box (SPMGR_11b8_08aa) to restore it before
// the next frame; the port draws the whole bar over each frame.

#include "core/dtable.hpp"
#include "core/font.hpp"
#include "core/surface.hpp"
#include "core/xform.hpp"
#include "engine/world/render.hpp"
#include "engine/world/world.hpp"

#include <array>

namespace crusader {

class StatusBar {
public:
    static constexpr int kBoxes = 5;
    static constexpr int kTop = 450;
    static constexpr std::uint16_t kBoxShape = 1;
    static constexpr std::array<std::uint16_t, kBoxes> kBoxFrames = {0, 1, 0, 2, 3};

    struct Rect {
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // inclusive
    };

    StatusBar(ShapeCache& gumps, const FontSet& fonts, const XformTable& xform, const dtable::NpcTypes& npc_types);

    const Rect& box(int i) const { return boxes_[i]; }

    // Draws over the view when GameState::status_bar_visible is set.
    void draw(Surface8& screen, const World& world) const;

    // What the boxes show, read from the world (exposed for tests).
    struct Contents {
        ObjId weapon = 0;
        std::uint16_t ammo_shape = 0;
        ObjId ammo = 0;
        ObjId item = 0;
        int health_width = -1;  // -1: no bar
        int energy_width = -1;
    };
    Contents contents(const World& world) const;

private:
    void draw_gump(Surface8& screen, std::uint16_t shape, std::uint16_t frame, int x, int y,
                   const XformTable* xform = nullptr) const;

    ShapeCache* gumps_;
    const FontSet* fonts_;
    const dtable::NpcTypes* npc_types_;
    std::array<Rect, kBoxes> boxes_{};
};

}  // namespace crusader
