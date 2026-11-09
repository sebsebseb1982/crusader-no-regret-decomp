#pragma once
// Damage to items that are not NPCs, explosions and splash damage
// (ITEMDAM.C segment 10C0, ITEM.C 275F, SPRITE 1150:0891, TRACKER.C
// 1128:039C). See docs/combat.md, "Objets touchés".
//
// STATIC/DAMAGE.FLX entry 0, 6 bytes per shape:
//   byte 0  bit 0 record present; bits 1-2 explosion when broken (size + 1);
//           bit 3 splash damage of that explosion; bit 4 the class's event 6
//           ("gotHit") decides when it breaks; bit 5 the explosion destroys
//           the item; bit 6 replaced by another shape when broken; bit 7 the
//           broken frame is taken from bytes 2..4 (else frame + byte)
//   byte 1  sound when broken
//   bytes 2-4  broken frames (bit 6: shape in 2-3, frame in 4)
//   byte 5  hit points given at creation (Item::hit_points)

#include "core/anim_dat.hpp"
#include "engine/kernel.hpp"
#include "engine/world/world.hpp"

#include <cstdint>

namespace crusader {

// ITEM_10a8_275f: an item that is not an NPC is hit by `other` with
// `force`. The hitter 0x4000 stands for damage: the force is then
// (fire type << 8) | damage; any other hitter (a contact of a move,
// 10E8:123A) brings no damage.
//  - A record without bit 4: the damage applies (damage_item); when the item
//    breaks, its processes stop.
//  - Otherwise, a record whose item would not break yet just loses hit
//    points. Then, if the class has event 6 ("gotHit"), the event runs with
//    the two words and, unless the item is parked (z 254), the damage
//    applies; with neither, nothing happens (CRUSADER_1130_1090 is empty).
constexpr ObjId kHitByDamage = 0x4000;
void got_hit(Kernel& kernel, World& world, const AnimDat& anims, ObjId item, ObjId other, std::uint16_t force);
// The same with damage of `fire_type`.
void hit_item(Kernel& kernel, World& world, const AnimDat& anims, ObjId item, int damage, int fire_type);

// ITEMDAM_10c0_030c. Returns true when the item breaks. Items flagged 0x100
// (already broken by damage) and shapes without a record are not affected.
// With hit points left, `damage` 0 does nothing and less damage than the
// hit points only takes some. Breaking: flag 0x100, Item::I_setBroken, the
// explosion of bits 1-2, then by bit 6 / 7 / neither: replacement shape
// (keeping the quality), a frame of bytes 2..4, or the frame plus one of
// bytes 2..4; with none of these bytes, an item broken by damage is
// destroyed.
bool damage_item(Kernel& kernel, World& world, const AnimDat& anims, ObjId item, int damage);

// SPRITE_1150_0891 (Item::I_explode with splash): `size` 0..2. Once per item
// (flag 0x40). Radius and damage are those of fire type 4 (10, 150) divided
// by 3, 2, 1 for sizes 0, 1, 2. With `splash`, splash damage of fire type 4
// around the item (the item excluded); with `destroy`, the item goes.
// Not reproduced: the sounds.
void explode(Kernel& kernel, World& world, const AnimDat& anims, ObjId item, int size, bool destroy, bool splash);

// TRACKER_1128_039c: items whose x, y lie within `radius` * 16 of `at`
// (grid order, ITEMFIND_10c8_18c0) take `damage` (at most 250; divided by
// the distance / 48 for fire types 3, 4 and 10), with the direction from
// `at` to them. Parked items, projectiles, `exclude` and `exclude2` are
// skipped, `exclude2` only when it is not the controlled NPC.
void splash_damage(Kernel& kernel, World& world, const AnimDat& anims, const World::Point& at, int radius,
                   int damage, int fire_type, ObjId exclude, ObjId exclude2);

}  // namespace crusader
