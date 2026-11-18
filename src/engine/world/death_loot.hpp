#pragma once
// Loot from remains (port addition, a cheat of the enhancements window,
// GameConfig::remains_loot, off by default). An enemy burnt, blown apart,
// melted or frozen leaves no body to search: NPCDEATH destroys what it
// carried and makes its remains (ashes, a skeleton, pieces...) without the
// loot a body gets. With the cheat, that loot drops on the floor beside the
// remains, as a body would have held it:
// - No Regret: what BOOTY's function 0x20 puts in a body (NPCDEATH 0x0A10
//   calls it once the body is made), picked by the shape the NPC had alive;
//   the NPC's own contents are already destroyed at its death (1140:04EF);
// - No Remorse: the weapons the NPC carried (NPCDEATH's function 0x21 moves
//   them into a body and destroys the rest), and what NPCDEATH's function
//   0x22 adds, picked by the shape as well.
//
// The shapes without a body are those NPCDEATH turns into remains or leaves
// as they are, without calling the loot function: No Regret 0x58F (burnt),
// 0x5D6 (frozen), 0x576 (burning), 0x596 (melted), 0x278, 0x656 (blown
// apart) and 0x62D; No Remorse 0x58F, 0x59C (electrocuted), 0x576 and 0x596.
//
// The death notes the shape the NPC had alive (note_death_loot). The loot
// drops when the original destroys it, as NPCDEATH starts: at the death
// animation's calledFromAnim event, or when the NPC enters the fast area (the
// two events whose usecode starts NPCDEATH); or at once, for a death shape
// whose animations call no event (No Remorse's 0x59C). The loot function
// draws the game's random numbers. The notes are not saved: a game saved
// during a death animation loses that NPC's loot.

#include "core/anim_dat.hpp"
#include "engine/kernel.hpp"
#include "engine/world/world.hpp"

#include <cstdint>

namespace crusader {

// The death shapes without a body of the game played (see above).
bool leaves_remains(const World& world, std::uint16_t shape);

// At the death of `npc`, which had `alive_shape` before it: with the cheat
// on, a death into one of the shapes without a body is noted for its loot,
// which drops at once when the death's animations call no event.
void note_death_loot(Kernel& kernel, World& world, const AnimDat& anims, ObjId npc, std::uint16_t alive_shape);

// Before a usecode event of `npc` that may start NPCDEATH (calledFromAnim,
// enterFastArea): the loot noted at its death drops beside it.
void drop_death_loot(Kernel& kernel, World& world, ObjId npc);

}  // namespace crusader
