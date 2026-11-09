#pragma once
// The two pseudo-random generators of CRUSADER.EXE.
//
// SYSTEM.C (12D8:04A0): a 32-bit linear congruential generator, state
// DS:4604 (low word) / DS:4606 (high word), 0x0BAD0BAD at start-up; each
// draw nearly multiplies by 65539 and adds 1 (the carries differ, see
// random_next), and gives the new high word. The saved
// games keep the state (player block, entry 20, offset 4) and loading sets
// it back with bit 0 forced (random_seed, 12D8:04E3). The usecode intrinsic
// rndRange and most engine draws use it.
//
// The Borland C library's rand() (1000:3775): state DS:6E1C, multiplier
// 0x015A4E35, plus 1, 15 bits from the high word. srand is never called, so
// it starts from 1 in every session and is not saved. The animation, combat
// and projectile code draws from it as rand() * n / 0x8000.

#include <cstdint>

namespace crusader {

constexpr std::uint32_t kRandomStartState = 0x0BAD0BAD;  // DS:4604 in the executable
constexpr std::uint32_t kCrtRandStartState = 1;           // DS:6E1C

// random_next: the next draw, 0..0xFFFF.
std::uint16_t random_next(std::uint32_t& state);

// random_range (12D8:04FF): 0..n-1, 0 for n = 0 (a draw is used either way).
std::uint16_t random_range(std::uint32_t& state, std::uint16_t n);

// UCMachine::I_rndRange (12E0:0293): lo..hi as signed bounds, in either
// order. A span of 0x10000 gives the lower bound.
std::int16_t random_between(std::uint32_t& state, std::int16_t lo, std::int16_t hi);

// random_seed (12D8:04E3): the state of a saved game.
std::uint32_t random_seed(std::uint32_t saved);

// The library's rand(): 0..0x7FFF.
std::uint16_t crt_rand(std::uint32_t& state);

// rand() * n / 0x8000, in 32 bits: 0..n-1 for 0 < n <= 0x10000.
int crt_rand_below(std::uint32_t& state, int n);

}  // namespace crusader
