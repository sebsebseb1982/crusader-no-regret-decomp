#pragma once
// Actor::I_createActorCru (ITEM.C 10A8:3D3F): an NPC made from a spawner
// item `egg` and a template item `kind`. See docs/combat.md, "Création de
// PNJ".
//
//   kind: NPC number = NPC type (DTABLE.FLX record: +2 hit points, +0x18
//         weapon, +0x1A other weapon, +0x3E shape); quality high = default
//         activity 0; map number = default activity 2.
//   egg:  quality low -> record +0x63; quality high = default activity 1;
//         map number bits 0-1 = lowest difficulty - 1, bit 2 = use the other
//         weapon; NPC number: low nibble = direction / 2, high nibble ->
//         record +0x12.
// Nothing is made above the game's difficulty. At difficulty 4 the weapon is
// drawn by shape (ITEM_10a8_3b10). The NPC (frame (direction + 4) & 15,
// record cleared, hit points of its type, combat tactic 0, flag 4 at +0x5A
// for shapes 0x597 and 0x3AC) is put where the egg is, with that place as
// its home (+0x5E), status 2, and its weapon created and made active.
// Returns the NPC, 0 when none (no free slot: with the alert on, the alarm
// lights are switched off and on again).

#include "core/anim_dat.hpp"
#include "core/dtable.hpp"
#include "engine/kernel.hpp"
#include "engine/world/world.hpp"

namespace crusader {

ObjId create_actor(Kernel& kernel, World& world, const AnimDat& anims, const dtable::NpcTypes& types, ObjId egg,
                   ObjId kind);

// ITEMFIND_10e0_0247: a monster egg (family 7) entering the fast area makes
// its NPC once (frame 0 -> 1). Like create_actor, with these sources: the
// egg's first content is the template (NPC number = type; quality high,
// quality low, map number = default activities 0, 1, 2); the egg's NPC
// number gives the direction (low nibble) and record +0x12 (high nibble),
// its map number bits 0-1 the lowest difficulty and bit 2 the other weapon.
// The NPC then takes its default activity 0 (Actor::I_setActivity(0x70));
// record +0x63 = 0. Returns the NPC (0 if none).
ObjId spawn_from_egg(Kernel& kernel, World& world, const AnimDat& anims, const dtable::NpcTypes& types, ObjId egg);

// Actor::I_createActor(shape, frame) (10F0:25B9): a new NPC of `shape`
// (World::create_npc, left on the pending stack) with the hit points of its
// type (NPC_10f0_2610), then its type's weapon, created where the NPC is and
// equipped in the first free slot of 1..5, and its default activities
// (NPC_10f0_0145, record words +0x1E, +0x40, +0x42). A shape that is no NPC
// type gets nothing (the original reads before the table). Returns the NPC.
ObjId create_plain_actor(World& world, const dtable::NpcTypes& types, std::uint16_t shape, std::uint16_t frame);

// ITEM_10a8_3b10: weapon (index in the weapon table) of a shape at
// difficulty 4.
int hard_weapon(World& world, std::uint16_t shape);

// Actor::I_setEquip (10F0:27FB): `item` goes into `npc`'s equipment slot
// (1..), when free (the first weapon becomes the active one; status 0x200).
bool set_equip(World& world, ObjId npc, int slot, ObjId item);

}  // namespace crusader
