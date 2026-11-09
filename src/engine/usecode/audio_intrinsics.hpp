#pragma once
// The sound intrinsics (ITEM.C 10A8:3934-39EB over SNDSRC, DSFXMAN.C over
// SNDMGR), numbered as in the French CRUSADER.EXE. They read the sound
// system from the world at each call and do nothing without one.

#include "engine/usecode/uc_machine.hpp"
#include "engine/world/world.hpp"

#include <cstdint>
#include <vector>

namespace crusader {

std::vector<std::uint16_t> register_audio_intrinsics(UCMachine& machine, World& world);

}  // namespace crusader
