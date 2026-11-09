#pragma once
// The status bar at the bottom of the screen (segment 11B8, "CruStatusGump" in
// ScummVM): five translucent boxes for the weapon, its ammunition, the active
// inventory item, health and energy.
//
// Layout (SPMGR_11b8_000c): box i is GUMPS.FLX shape 1, frame {0, 1, 0, 2, 3}[i]
// (table DS:2DAD), its top-left corner at x = 15 + 127 * i, y = 450. The
// spacing is meant to be (640 - box width) / 5, but the width is read before
// the box rectangle is filled in, while it is still all zeros: it is always
// (640 - 1) / 5 = 127, and the margin 127 / 8 = 15.
//
// Drawing (SPMGR_11b8_0192), per box, over the world view:
// - background, translucent: boxes 0 and 1 only while the player (NPC 1) is
//   the controlled NPC and has a weapon (box 1: a weapon that takes
//   ammunition), boxes 2 and 4 while the player is controlled, box 3 always;
// - nothing more unless the player is controlled, except for box 3;
// - box 0: the weapon icon (GUMPS.FLX shape 3) at the centre of the box;
// - box 1 (font 15): the weapon's quality (rounds loaded) at (+30, +11), and
//   at (+75, +11) the quality of the first item of the player's equipment
//   that is that ammunition, or "0";
// - box 2: the item icon at the centre (shape 3 frame 13 for the credits 0x4ED,
//   shape 4 for family 12, shape 5 otherwise; icons 3, 5 and 7 of shape 5
//   become 18, 17 and 19 when the item's frame is not 0), and its quality at
//   (+75, +18) in font 12 when above 1;
// - box 3: a health bar of colour 0x41, (+34, +7) to (+34 + w, +20) inclusive,
//   w = (uint16)(hp * 66) / max hp, max hp from DTABLE.FLX, if hp is not 0;
// - box 4: an energy bar of colour 0xF5, w = energy * 66 / 10000.
// Weapon and item come from the player's record (DS:7E0E holds the player's
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
    const XformTable* xform_;
    const dtable::NpcTypes* npc_types_;
    std::array<Rect, kBoxes> boxes_{};
};

}  // namespace crusader
