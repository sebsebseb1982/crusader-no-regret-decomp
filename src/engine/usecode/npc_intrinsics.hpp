#pragma once
// NPC activity and combat intrinsics (NPC.C), numbered as in the French
// CRUSADER.EXE. They need the animations, unlike the item intrinsics.

#include "core/anim_dat.hpp"
#include "core/dtable.hpp"
#include "engine/usecode/uc_machine.hpp"
#include "engine/world/world.hpp"

#include <cstdint>
#include <vector>

namespace crusader {

// `types` (DTABLE.FLX) is needed by Actor::I_createActorCru (0 = not bound).
std::vector<std::uint16_t> register_npc_intrinsics(UCMachine& machine, World& world, const AnimDat& anims,
                                                   const dtable::NpcTypes* types = nullptr);

}  // namespace crusader
