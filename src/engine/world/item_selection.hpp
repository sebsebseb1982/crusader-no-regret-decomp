#pragma once
// The item selection mode of the game view (WORLD.C 13E8:33D9 and around):
// keys S and G of World_on_key (player_commands.hpp), keypad + (an 's').
// The state is GameState::selection.
//
// Starting (13E8:33D9): the list holds, from items 0x100..0x27FF, those
// not parked (z 254), not contained (status 8) nor flagged 0x1000, of shape
// 0x4ED, of families 10..13 (weapons, ammunition, inventory items) or with
// the "selectable" type flag (TYPEFLAG byte 6 bit 1: switches, computers...),
// whose centre (getCX / getCY) is less than 0x80 from the player's
// (ITEMFIND_10d8_0098: the larger of |dx| and |dy|), with a z between the
// player's - 0x18 and + 0x50 (both excluded). Sorted by distance, nearest
// first; the nearest is selected and framed by the sprite 0x5A3 frame 0
// (SPMGR_11a8_0115, drawn over the scene at the item's centre, following
// it). An empty list: sound 0xB0 and the mode ends at once.
//
// S selects; then (World_on_key with flag 0x100, all other keys ignored):
//   . > s S  the next item (wrapping)       , <  the previous (wrapping)
//   Enter, Space, Ins  a selectable item is used (Item_I_use), another is
//                      picked up (MainActor_I_addItemCru, shown); the mode ends
//   U  the item is used if its shape is not an inventory item (DTABLE
//      1120:0424), the mode ends
//   Escape, arrows  the mode ends
// G picks up everything: at each pass of the game view, the selected item
// is picked up and the next selected; the mode ends after the last. Keys
// are ignored meanwhile, unless the player may not act.
// The mode also ends (nothing picked up) when the controlled NPC starts an
// animation (Actor::I_doAnim: walking, firing...) and on Z (WORLD_13e8_007d).
//
// Not reproduced: G runs a modal loop (NEWGUMP_1368_0000 and the view's
// vfunc +4) until the list ends, and calls the camera's vfunc 0 before each
// pick (meaning unknown); the port picks one item per frame without
// blocking.

#include "engine/kernel.hpp"
#include "engine/world/world.hpp"

#include <cstdint>
#include <optional>

namespace crusader {

// 13E8:33D9: `pick_all` for G.
void start_selection(World& world, bool pick_all);
// 13E8:3796: the mode ends, nothing picked up.
void cancel_selection(World& world);
// A key code of the game view while the mode is on.
void selection_key(World& world, std::uint16_t code);
// A press of the left mouse button (13E8:2B74 with flag 0x100, not a double
// click): as Enter.
void selection_click(World& world);
// The game view's pass (13E8:2xxx, flag 0x100): the frame follows the
// item; G picks it up.
void selection_tick(World& world);
// The selected item, where the frame (shape 0x5A3) is drawn.
ObjId selected_item(const World& world);

constexpr std::uint16_t kSelectionFrameShape = 0x5A3;

}  // namespace crusader
