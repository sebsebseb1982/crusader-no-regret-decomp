#include "engine/ui/status_bar.hpp"

#include "engine/world/actor_anim.hpp"

#include <string>

namespace crusader {

namespace {

constexpr ObjId kPlayer = 1;               // DS:7E0E
constexpr std::uint16_t kCredits = 0x4ED;  // shown with shape 3 frame 13
constexpr int kBarLength = 66;
constexpr std::uint8_t kHealthColour = 0x41;
constexpr std::uint8_t kEnergyColour = 0xF5;
constexpr int kAmmoFont = 15;
constexpr int kItemFont = 12;

// Equipment slot `slot_field` of the player's record (NPC_10f0_33b8 / 3420):
// slot 0 means none.
ObjId equipped(const World& world, std::size_t slot_field) {
    const int slot = world.npc_word(kPlayer, slot_field);
    return slot == 0 ? 0 : world.npc_equip(kPlayer, slot);
}

// NPC_10f0_395e: first item of the player's 30 equipment slots with this shape.
// @orig 10F0:395E
ObjId find_equipped_shape(const World& world, std::uint16_t shape) {
    for (int slot = 0; slot < 30; ++slot) {
        const ObjId id = world.npc_equip(kPlayer, slot);
        if (id == 0) continue;
        if (const Item* it = world.get(id); it && it->shape == shape) return id;
    }
    return 0;
}

int center(int a0, int a1) { return (((a1 - a0) + 1) >> 1) + a0; }

// Qualities are printed with "%d" (DS:2DB8) and compared as signed words.
int signed_quality(const Item& item) { return static_cast<std::int16_t>(item.quality); }

}  // namespace

// @orig 11B8:000C
StatusBar::StatusBar(ShapeCache& gumps, const FontSet& fonts, const XformTable& xform,
                     const dtable::NpcTypes& npc_types)
    : gumps_(&gumps), fonts_(&fonts), xform_(&xform), npc_types_(&npc_types) {
    const int spacing = (640 - 1) / kBoxes;  // see the header: width read as 1
    for (int i = 0; i < kBoxes; ++i) {
        const Shape* shape = gumps_->get(kBoxShape);
        const ShapeFrame* f = shape && kBoxFrames[i] < shape->frames().size() ? &shape->frames()[kBoxFrames[i]] : nullptr;
        const int w = f ? f->width : 1;
        const int h = f ? f->height : 1;
        const int x = (spacing >> 3) + spacing * i;
        boxes_[i] = {x, kTop, x + w - 1, kTop + h - 1};
    }
}

void StatusBar::draw_gump(Surface8& screen, std::uint16_t shape, std::uint16_t frame, int x, int y,
                          const XformTable* xform) const {
    const Shape* s = gumps_->get(shape);
    if (!s || frame >= s->frames().size()) return;
    s->frames()[frame].draw(screen, x, y, false, xform);
}

// @orig 11B8:0192
StatusBar::Contents StatusBar::contents(const World& world) const {
    Contents c;
    c.weapon = equipped(world, npc::kWeapon);
    if (c.weapon) {
        if (const Item* w = world.get(c.weapon)) {
            c.ammo_shape = dtable::ammo_shape(w->shape);
            if (c.ammo_shape) c.ammo = find_equipped_shape(world, c.ammo_shape);
        }
    }
    c.item = equipped(world, npc::kActiveItem);

    const ObjId controlled = world.state.controlled_npc;
    if (const std::uint16_t hp = world.npc_hp(controlled)) {
        const Item* npc = world.get(controlled);
        const std::uint16_t max_hp = npc_types_->max_hp(npc_types_->index(npc ? npc->shape : 0));
        // 16-bit multiply, then unsigned divide (11B8:0747). An NPC shape
        // missing from DTABLE.FLX would make the original read before the
        // table; the port draws an empty bar.
        const auto scaled = static_cast<std::uint16_t>(hp * kBarLength);
        c.health_width = max_hp ? scaled / max_hp : 0;
    }
    if (const std::uint16_t energy = world.npc_mana(controlled)) {
        c.energy_width = static_cast<int>(std::uint32_t{energy} * kBarLength / 10000);
    }
    return c;
}

// @orig 11B8:0192
void StatusBar::draw(Surface8& screen, const World& world) const {
    if (!world.state.status_bar_visible) return;
    const bool player = world.state.controlled_npc == kPlayer;
    const Contents c = contents(world);

    for (int i = 0; i < kBoxes; ++i) {
        const Rect& r = boxes_[i];
        bool background = false;
        switch (i) {
        case 0: background = player && c.weapon; break;
        case 1: background = player && c.weapon && c.ammo_shape; break;
        default: background = player || i == 3; break;
        }
        if (background) draw_gump(screen, kBoxShape, kBoxFrames[i], r.x0, r.y0, xform_);
        if (!player && i != 3) continue;

        switch (i) {
        case 0:
            if (const Item* w = c.weapon ? world.get(c.weapon) : nullptr) {
                draw_gump(screen, 3, dtable::weapon_icon(w->shape), center(r.x0, r.x1), center(r.y0, r.y1));
            }
            break;
        case 1:
            if (const Item* w = c.weapon && c.ammo_shape ? world.get(c.weapon) : nullptr) {
                TextWriter t(*fonts_, kAmmoFont);
                t.print(screen, r.x0 + 30, r.y0 + 11, std::to_string(signed_quality(*w)));
                const Item* ammo = c.ammo ? world.get(c.ammo) : nullptr;
                t.print(screen, r.x0 + 75, r.y0 + 11, ammo ? std::to_string(signed_quality(*ammo)) : "0");
            }
            break;
        case 2:
            if (const Item* it = c.item ? world.get(c.item) : nullptr) {
                std::uint16_t shape = 5, frame = 0;
                if (it->shape == kCredits) {
                    shape = 3;
                    frame = 13;
                } else if (world.family_of(it->shape) == 12) {
                    shape = 4;
                    frame = dtable::item_icon_family12(it->shape);
                } else {
                    frame = dtable::item_icon(it->shape);
                    if (it->frame != 0) {
                        if (frame == 3) frame = 0x12;
                        else if (frame == 5) frame = 0x11;
                        else if (frame == 7) frame = 0x13;
                    }
                }
                draw_gump(screen, shape, frame, center(r.x0, r.x1), center(r.y0, r.y1));
                if (signed_quality(*it) > 1) {
                    TextWriter(*fonts_, kItemFont).print(screen, r.x0 + 75, r.y0 + 18, std::to_string(signed_quality(*it)));
                }
            }
            break;
        case 3:
            if (c.health_width >= 0) {
                fill_rect(screen, r.x0 + 34, r.y0 + 7, r.x0 + 34 + c.health_width, r.y0 + 20, kHealthColour);
            }
            break;
        case 4:
            if (c.energy_width >= 0) {
                fill_rect(screen, r.x0 + 34, r.y0 + 7, r.x0 + 34 + c.energy_width, r.y0 + 20, kEnergyColour);
            }
            break;
        }
    }
}

}  // namespace crusader
