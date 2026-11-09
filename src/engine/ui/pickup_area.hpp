#pragma once
// Drawing of the items just picked up (DISPLAY_1190_010f), after the status
// bar, unless the game is in the menu mode (DS:2CEE, set with DS:2D8F by
// World_enter_menu_mode). Entry i (pickup_list.hpp) is a strip at x = 4,
// y = 32 * i + 4, 28 pixels high:
// - its width W (DISPLAY_1190_049c): the icon's width - 1, plus the name's
//   width in font 13, plus 15, rounded down to a multiple of 28, plus 56;
// - background: GUMPS.FLX shape 2 frame 0, translucent, every 28 pixels
//   while below W;
// - the icon (not translucent) at (+33, +14);
// - the count in font 12 at (+53, +20) when above 1 ("%d", DS:2CEF);
// - the name in font 13 at (+66, +10).
// The original draws an entry only when it changed or a dirty rectangle of
// the frame overlaps it, and saves the view under it (8 viewports of
// 199 x 27, DISPLAY_1190_0093) to restore it afterwards (0394); the port
// draws every entry over each frame.

#include "core/font.hpp"
#include "core/surface.hpp"
#include "core/xform.hpp"
#include "engine/world/pickup_list.hpp"
#include "engine/world/render.hpp"

namespace crusader {

class PickupArea {
public:
    static constexpr int kLeft = 4;
    static constexpr int kTop = 4;
    static constexpr int kSpacing = 32;
    static constexpr int kTile = 28;

    PickupArea(ShapeCache& gumps, const FontSet& fonts, const XformTable& xform);

    // Width of the strip of an entry (W above).
    int width(const PickupEntry& entry) const;
    void draw(Surface8& screen, const PickupList& list) const;

private:
    void draw_gump(Surface8& screen, std::uint16_t shape, std::uint16_t frame, int x, int y,
                   const XformTable* xform) const;

    ShapeCache* gumps_;
    const FontSet* fonts_;
    const XformTable* xform_;
};

}  // namespace crusader
