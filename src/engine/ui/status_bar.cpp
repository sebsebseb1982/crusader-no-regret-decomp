#include "engine/ui/status_bar.hpp"

#include "engine/world/inventory.hpp"
#include "engine/world/actor_anim.hpp"
#include "engine/world/pickup_list.hpp"

#include <string>

namespace crusader {

namespace {

constexpr ObjId kPlayer = 1;               // DS:898C
constexpr std::uint8_t kHealthColour = 0x41;
constexpr std::uint8_t kEnergyColour = 0xF5;
constexpr int kAmmoFont = 8;    // 11C8:0A3B (No Remorse: 15)
constexpr int kItemFont = 12;
constexpr int kNameFont = 18;   // 11C8:068C, 08F6
constexpr std::uint16_t kFrozen = 0x5D6;   // a frozen body has no health bar
constexpr std::uint16_t kMessage = 0x4D4;  // "MESSAGE" instead of "ACTIVE"
// Bars: (+0x23, +8) to (+0x23 + w, +0x13) inclusive (No Remorse: +0x22, +7, +0x14).
constexpr int kBarLeft = 0x23;
constexpr int kBarTop = 8;
constexpr int kBarBottom = 0x13;

// Equipment slot `slot_field` of the player's record (NPC_10f0_33b8 / 3420):
// slot 0 means none.
ObjId equipped(const World& world, std::size_t slot_field) {
    const int slot = world.npc_word(kPlayer, slot_field);
    return slot == 0 ? 0 : world.npc_equip(kPlayer, slot);
}

// NPC_10f0_395e: first item of the player's 30 equipment slots with this shape.
// @orig 10F8:4056
ObjId find_equipped_shape(const World& world, std::uint16_t shape) {
    for (int slot = 0; slot < world.equip_slots(); ++slot) {
        const ObjId id = world.npc_equip(kPlayer, slot);
        if (id == 0) continue;
        if (const Item* it = world.get(id); it && it->shape == shape) return id;
    }
    return 0;
}

int center(int a0, int a1) { return (((a1 - a0) + 1) >> 1) + a0; }

// Text whose right end is 4 pixels left of the box's right edge:
// x = x1 - (width + 4) (12B0:02AA gives the width).
void print_right(Surface8& screen, const FontSet& fonts, int font, int x1, int y, std::string_view text) {
    TextWriter t(fonts, font);
    t.print(screen, x1 - (t.measure(text).width + 4), y, text);
}

// Qualities are printed with "%d" (DS:2DB8) and compared as signed words.
int signed_quality(const Item& item) { return static_cast<std::int16_t>(item.quality); }

}  // namespace

// No Remorse sets byte +4 of each box's shape reference to 1 (translucent);
// No Regret leaves it 0: the boxes are drawn opaque (1338:0109).
// @orig 11C8:0222
StatusBar::StatusBar(ShapeCache& gumps, const FontSet& fonts, const XformTable& /*xform*/,
                     const dtable::NpcTypes& npc_types)
    : gumps_(&gumps), fonts_(&fonts), npc_types_(&npc_types) {
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

// @orig 11C8:03A3
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
    const Item* npc = world.get(controlled);
    if (const std::uint16_t hp = world.npc_hp(controlled); hp && !(npc && npc->shape == kFrozen)) {
        const std::uint16_t max_hp = npc_types_->max_hp(npc_types_->index(npc ? npc->shape : 0));
        // 16-bit shift, then unsigned divide (11C8:0B50). An NPC shape
        // missing from DTABLE.FLX would make the original read before the
        // table; the port draws an empty bar.
        const auto scaled = static_cast<std::uint16_t>(hp << 6);
        c.health_width = max_hp ? scaled / max_hp : 0;
    }
    if (const std::uint16_t energy = world.npc_mana(controlled)) {
        c.energy_width = static_cast<int>((std::uint32_t{energy} << 6) / 10000);  // 11C8:0BF8
    }
    return c;
}

// @orig 11C8:03A3
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
        if (background) draw_gump(screen, kBoxShape, kBoxFrames[i], r.x0, r.y0);
        if (!player && i != 3) continue;

        switch (i) {
        case 0:
            if (const Item* w = c.weapon ? world.get(c.weapon) : nullptr) {
                draw_gump(screen, 3, dtable::weapon_icon(w->shape), center(r.x0, r.x1), center(r.y0, r.y1));
                if (const char* name = item_label(world, *w).name) {  // 11C8:066C
                    print_right(screen, *fonts_, kNameFont, r.x1, r.y0 + 0x14, name);
                }
            }
            break;
        case 1:
            if (const Item* w = c.weapon && c.ammo_shape ? world.get(c.weapon) : nullptr) {
                TextWriter t(*fonts_, kAmmoFont);
                t.print(screen, r.x0 + 38, r.y0 + 8, std::to_string(signed_quality(*w)));  // 11C8:0A5C
                const Item* ammo = c.ammo ? world.get(c.ammo) : nullptr;
                t.print(screen, r.x0 + 90, r.y0 + 8, ammo ? std::to_string(signed_quality(*ammo)) : "0");  // 11C8:0AB0
            }
            break;
        case 2:
            if (const Item* it = c.item ? world.get(c.item) : nullptr) {
                std::uint16_t shape = 5, frame = 0;
                if (it->shape == credits_shape(world)) {  // shown with shape 3 frame 13
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
                draw_gump(screen, shape, frame, center(r.x0, r.x1) - 0x1C, center(r.y0, r.y1));  // 11C8:0833
                if (signed_quality(*it) > 1) {  // 11C8:0864
                    print_right(screen, *fonts_, kItemFont, r.x1, r.y0 + 0x14, std::to_string(signed_quality(*it)));
                }
                if (const char* name = item_label(world, *it).name) {  // 11C8:08D6
                    print_right(screen, *fonts_, kNameFont, r.x1, r.y0 + 4, name);
                    if (it->frame == 1) {  // DS:36C9, DS:36D1
                        print_right(screen, *fonts_, kNameFont, r.x1, r.y0 + 0xA, it->shape == kMessage ? "MESSAGE" : "ACTIVE");
                    }
                }
            }
            break;
        case 3:
            if (c.health_width >= 0) {
                fill_rect(screen, r.x0 + kBarLeft, r.y0 + kBarTop, r.x0 + kBarLeft + c.health_width, r.y0 + kBarBottom,
                          kHealthColour);
            }
            break;
        case 4:
            if (c.energy_width >= 0) {
                fill_rect(screen, r.x0 + kBarLeft, r.y0 + kBarTop, r.x0 + kBarLeft + c.energy_width, r.y0 + kBarBottom,
                          kEnergyColour);
            }
            break;
        }
    }
}

}  // namespace crusader
