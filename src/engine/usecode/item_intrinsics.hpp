#pragma once
// Item intrinsics called by usecode (`calli`), numbered as in the French
// CRUSADER.EXE (table USECODE/UNKCOFF.DAT; names from ScummVM's
// RemorseFrIntrinsics). Several numbers alias the same engine function.

#include "engine/usecode/uc_machine.hpp"
#include "engine/world/world.hpp"

#include <cstdint>
#include <vector>

namespace crusader {

// Item::I_destroy: removes the item (and what it holds) with the original's side effects.
void destroy_item(Kernel& kernel, World& world, ObjId id);

// Binds the item intrinsics to `world`. Returns the intrinsic numbers bound.
std::vector<std::uint16_t> register_item_intrinsics(UCMachine& machine, World& world);

}  // namespace crusader
