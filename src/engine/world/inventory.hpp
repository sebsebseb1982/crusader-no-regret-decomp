#pragma once
// The player's inventory (WORLD.C 13E8:0330); equipping is set_equip
// (actor_create.hpp, NPC.C 10F0:27FB).
//
// An NPC's equipment is 45 slots (30 in No Remorse) of item ids in its record (+0x1B); the
// player's are used by kind: 1..5 weapons (family 10), 6..10 ammunition
// (family 11), 11..25 other items (families 12 and 13, and shape 0x4ED).
// The active weapon's slot is at +0x15, the active item's at +0x19. An
// equipped item is in the NPC's contents, with its slot as z and flag 0x200.
//
// MainActor::I_addItemCru(item, show) picks an item up; true if taken:
//   - 0x4ED: added to the quality of the one carried, else in a free item slot;
//   - weapons: refused when one of the shape is carried; else in a free
//     weapon slot, loaded with a full clip (0 rounds for a weapon without
//     ammunition), and active when none was;
//   - ammunition: one more clip on those carried (up to 20), else in a free
//     slot with quality 1;
//   - keycard 0x111: bit (quality low byte, below 32) of the keycards;
//   - batteries 0x3A2 / 0x3A4 / 0x3A3 raise the battery to 1 / 2 / 3 and
//     refill the energy, or when it is already that good give 2500 / 5000 /
//     10000 energy;
//   - other items: one more on those carried (up to 10, or 20 for 0x560;
//     shields 0x52E..0x530 carried: just destroyed); shields raise the
//     shield level (+0x66) to 1 / 2 / 3; else in a free item slot, quality 1,
//     made the active item.
// The picked-up item is destroyed when it only adds to another.
// When `show`, the item is listed at the top left of the screen
// (GameState::pickups, DISPLAY_1190_049c) before it is destroyed; never for
// the datalink 0x4D4 added as a new item or to those carried. The original
// lists a shield after destroying it (it reads the shape of a destroyed
// item); the port lists it before.
// Not reproduced: the status bar boxes marked for redrawing (the port redraws
// the whole bar).

#include "engine/world/world.hpp"

namespace crusader {

// Item::I_setQAndCombine: the quality, then usecode event 0x0C ("combine").
void set_quality_and_combine(World& world, ObjId item, std::uint16_t quality);

bool add_item_cru(World& world, ObjId item, bool show);

// NPC_10f0_395e: the first of the player's 30 equipment slots holding
// `shape`, 0 if none.
ObjId carried(const World& world, std::uint16_t shape);

// The credits: shape 0x112 in No Regret, 0x4ED in No Remorse (No Regret's
// 0x4ED is the VIR imager).
inline std::uint16_t credits_shape(const World& world) { return world.remorse() ? 0x4ED : 0x112; }

// The player's equipment slots by kind (first and last slot). No Regret
// (NPC 10F8:31C1, 31EA, 321A): weapons 1..12, ammunition 13..20, items
// 21..44. No Remorse: weapons 1..5, ammunition 6..10, items 11..25.
struct SlotRange {
    int first, last;
};
inline SlotRange weapon_slots(const World& world) { return world.remorse() ? SlotRange{1, 5} : SlotRange{1, 12}; }
inline SlotRange ammo_slots(const World& world) { return world.remorse() ? SlotRange{6, 10} : SlotRange{13, 20}; }
inline SlotRange item_slots(const World& world) { return world.remorse() ? SlotRange{11, 25} : SlotRange{21, 44}; }

// No Regret's weapons (DS:2F5F), in the order of the digit keys 1..0, - and =
// (CONTROL_1148_09fd) and of the F10 cheat.
inline constexpr std::array<std::uint16_t, 12> kRegretWeapons = {0x334, 0x332, 0x38B, 0x38A, 0x38D, 0x38C,
                                                                  0x5F6, 0x198, 0x5F5, 0x38E, 0x388, 0x386};

// NPC 10F8:31C1 / 31EA / 321A: the first empty slot of the player's slots
// of a range, 0 if none.
int free_equip_slot(const World& world, SlotRange range);

// WORLD_13e8_00c8 (only for Weasel's shop): whether the player could take an
// item of `shape` (the original is given an item and reads its shape and
// family): the credits 0x4ED while those carried stay below 60000, or with a
// free item slot; a weapon with a free weapon slot; an ammunition while the
// one carried is below 20, or with a free ammunition slot; for families 12
// and 13, always the keycard 0x111 and the batteries 0x3A2..0x3A4, else while
// the one carried is a shield 0x52E..0x530, or below 20 (0x560) or 10, or
// with a free item slot. Never for the other families.
bool may_add_item(const World& world, std::uint16_t shape);

// Item::I_use (10A8:2667): usecode event 1 ("use") of the item's class, when
// the class has it. Refused for a dead NPC, and for an NPC in combat other
// than the player. may_use says whether the event may start.
bool may_use(const World& world, ObjId item);
void use_item(World& world, ObjId item);

// Player record fields.
constexpr std::size_t kNpcActiveWeapon = 0x15;
constexpr std::size_t kNpcActiveItem = 0x19;
constexpr std::size_t kNpcShieldLevel = npc::kShieldLevel;

}  // namespace crusader
