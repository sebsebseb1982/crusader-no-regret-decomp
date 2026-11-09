#pragma once
// The player's inventory keys of World_on_key (WORLD.C 13E8:14B4), the key
// handler of the game view. It receives the gump key codes (gump_keys.hpp):
//   Q / q  previous weapon (13E8:0C07)    W / w  next weapon (13E8:0C72)
//   I / i  previous item (13E8:0D5F)      O / o  next item (13E8:0DC0)
//   U / u  use the active item            M / m  use a medikit (0x351)
//   E / e  use an energy cube (0x582)     B / b  set off the detpacs (0x55F)
//   Ctrl+D (0x420)  drop the active weapon at the player's feet
// They act only when the player may act: not in stasis (DS:7E10), alive and
// the controlled NPC; M and E also not in "cru stasis" (DS:5FFB).
//
// The player's equipment slots (inventory.hpp): weapons 1..5, items 11..25.
// Q / W turn through the weapon slots and I / O through the item slots, from
// the active one (none: nothing happens), to the next slot holding something,
// wrapping (the current slot last). Going up the items wrap after slot 24,
// so slot 25 is only reached going down. W also needs the player to be the
// controlled NPC (DS:2852 = 1) and changes his pose to fit the new weapon
// when he is not idle: kneeling 0x2E, else, when his last action is not one
// of 0, 1, 0xB, 0x10, 0x20, 0x37, 0x3A, 0x3B (CRUSADER_1138_0014), the
// combat stance 4; 0x2F and 0x25 instead with a weapon whose record does not
// keep the actions (NPC_10f0_3b57, the pairs of remap_action). Never while
// he has an animation.
//
// U uses the active item (Item_I_use), except shape 0x4ED. Then, other
// than the datalink 0x4D4 and the 0x52D..0x530 scanner and shields, it is
// used up: quality - 1 (Item_I_setQAndCombine), and at 0 the item is
// destroyed, leaves its slot and the next item becomes active. M and E do
// the same with the first medikit / energy cube carried, but the next item
// never becomes active: the original compares the active item with the
// used one after emptying its slot (an original quirk, kept). The item
// scripts do the work: the medikit's gives 80 hit points, or, when the
// player has all his hit points, a new medikit back.
//
// B uses every detpac in the square of 0x800 units around the player
// (AreaSearch_init with the search script "@ %0x55F = $").
//
// G and S (when the player may act and not in "cru stasis") clear the
// "Crusader teleporting" flag (DS:6000) and start the item selection mode,
// which then takes every key (item_selection.hpp).
//
// R (no condition) toggles target designation (DS:2889) and says so in the
// message box (DS:604C / 6069). It governs the target marker of the aiming
// mode (aim_mode.hpp).
//
// The function keys and Alt+X open screens, so the game handles them
// (Game::function_key).
//
// Ctrl+D: the player must be the controlled NPC (ITEM_10a8_00c1). The weapon
// 0x32E (the first of the weapon table) is kept below difficulty 2. The next
// weapon becomes active (W, with its pose), then the weapon is pushed out of
// the player (leaving its slot) and popped at the player's coordinates.
//
// Not reproduced: the status bar boxes marked for redrawing
// (SPMGR_11b8_09ba; the port redraws the whole bar).
//
// Tab and J (the Controller, 1138:253F) make the player jump, when he is the
// controlled NPC: action 0x3A (a standing jump forward, about 110 units, up
// to 24 high) when idle (CRUSADER_1138_0014), 0x38 (the same without the
// take-off) when running (last action 0x38, or Actor_is_running), else the
// weapon is put away first (0xB) and 0x3A follows. All refused while he has
// an animation, except a run alone (one animation, not already 0x38): it
// fails and 0x38 starts.

#include "core/anim_dat.hpp"
#include "engine/kernel.hpp"
#include "engine/world/world.hpp"

#include <cstdint>

namespace crusader {

// The player may act: World_on_key's first test.
bool player_may_act(const World& world);

void previous_weapon(World& world);
void next_weapon(Kernel& kernel, World& world, const AnimDat& anims);
void previous_item(World& world);
void next_item(World& world);
void use_active_item(World& world);
void use_carried(World& world, std::uint16_t shape);
void set_off_detpacs(World& world);
void drop_weapon(Kernel& kernel, World& world, const AnimDat& anims);
void jump(Kernel& kernel, World& world, const AnimDat& anims);

// The keys above: true when `code` is one of them (acted on or not).
bool player_command(Kernel& kernel, World& world, const AnimDat& anims, std::uint16_t code);

}  // namespace crusader
