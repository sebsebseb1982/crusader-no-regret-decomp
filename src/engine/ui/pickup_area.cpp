#include "engine/ui/pickup_area.hpp"

#include <string>

namespace crusader {

namespace {

constexpr int kNameFont = 13;
constexpr int kCountFont = 12;
constexpr std::uint16_t kBackShape = 2;

}  // namespace

PickupArea::PickupArea(ShapeCache& gumps, const FontSet& fonts, const XformTable& xform)
    : gumps_(&gumps), fonts_(&fonts), xform_(&xform) {}

void PickupArea::draw_gump(Surface8& screen, std::uint16_t shape, std::uint16_t frame, int x, int y,
                           const XformTable* xform) const {
    const Shape* s = gumps_->get(shape);
    if (!s || frame >= s->frames().size()) return;
    s->frames()[frame].draw(screen, x, y, false, xform);
}

// GumpShape_rect, then the name's width (HUGE_12c0_02aa).
// @orig 1190:049C
int PickupArea::width(const PickupEntry& entry) const {
    const Shape* s = gumps_->get(entry.icon_shape);
    const int icon = s && entry.icon_frame < s->frames().size() ? s->frames()[entry.icon_frame].width : 1;
    const int right = icon - 1 + TextWriter(*fonts_, kNameFont).measure(entry.name).width + 15;
    return (right / kTile + 2) * kTile;
}

// @orig 1190:010F
void PickupArea::draw(Surface8& screen, const PickupList& list) const {
    for (int i = 0; i < PickupList::kEntries; ++i) {
        const PickupEntry& e = list.entries()[static_cast<std::size_t>(i)];
        if (!e.name) continue;
        const int x = kLeft;
        const int y = kTop + kSpacing * i;
        const int w = width(e);
        for (int dx = 0; dx < w; dx += kTile) draw_gump(screen, kBackShape, 0, x + dx, y, xform_);
        draw_gump(screen, e.icon_shape, e.icon_frame, x + 33, y + 14, nullptr);
        if (e.count > 1) TextWriter(*fonts_, kCountFont).print(screen, x + 53, y + 20, std::to_string(e.count));
        TextWriter(*fonts_, kNameFont).print(screen, x + 66, y + 10, e.name);
    }
}

}  // namespace crusader
