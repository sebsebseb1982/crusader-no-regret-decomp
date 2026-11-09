#pragma once
// The healing and energy stations' processes (CRUSADER.C 1130:1D70-2025),
// started by their scripts with CruHealerProcess::I_create (0x0F6) and
// BatteryChargerProcess::I_create (0x0EE). Type 0x254, owned by the player
// (DS:7E0E), which they flag as being served (DS:7E10) while they run.
//
// - CruHealer: the target is the player's maximum hit points (150,
//   NPC_10f0_3375); each tick, while the hit points differ, one more (not
//   past the target), with sound 0xBA (HEAL1) unless it already plays.
// - BatteryCharger: the target is the maximum energy of the battery
//   (MainActor::I_getMaxEnergy); each tick 25 more, with sound 0x10B
//   (RECHARGE).
// Both loop a sound at the player while they run (0xDB HEALING, 0xA4
// FUSNCORE), stopped at the end.
//
// Not reproduced: the status bar flash of each tick (SPMGR_11b8_09ba 3 / 4).

#include "engine/kernel.hpp"
#include "engine/world/world.hpp"

#include <cstdint>

namespace crusader {

// MainActor::I_getMaxEnergy (10F0:348B): by battery (DS:7E1F) 0, 2500,
// 5000, 10000; 0 for other NPCs.
int max_energy(const World& world, ObjId npc);

// The two intrinsics: start the process, return its pid.
Pid start_healer(Kernel& kernel, World& world);
Pid start_battery_charger(Kernel& kernel, World& world);

}  // namespace crusader
