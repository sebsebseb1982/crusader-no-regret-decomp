#pragma once
// Palette colour cycling (segment 1438): "CycleProcess", type 0x248, created
// once when the game starts (11D8:342C, after the game palette is written).
//
// A timed process at 30 Hz that runs at any level. At each tick, unless
// paused (+0x40, GameState::cycle_paused) or +0x41 is set (never in this
// version), UNITFILE_1438_011B changes the game palette (DS:058C) in place:
// - entries 1..7 rotate: each takes the next one's colour, 7 takes 1's;
// - seven colours (DS:6845, 3 bytes each, kept between ticks) go to the
//   entries of DS:6814 (8..14). Mode 0 (DS:6822, entries 8..13): the
//   components flagged in DS:6830 go up by 2, back to 0 past 63. Mode 1
//   (entry 14): the same, and when a component wraps every component then
//   gets random_range(10) more, masked to 63;
// - entries 1..14 of the game palette are written to the DAC.
// Initial values (static initialiser UNITFILE_1438_0480):
//   entry  8  9  10  11  12  13  14
//   flags  R  B  R   B   RG  RGB G
//   start  0  0  31R 31B 0   0   0
//
// Paused (DS:6810 + 0x40) by fades while they run (see palette_fader.hpp),
// the palette jumps to black, white or grey, the menu, a cutscene, starting a
// game and changing map; resumed by jumpToNormalPalette, the end of
// fadeToGamePal / fadeFromBlack (their waiting process, 1440:0F2F), closing
// the menu, the end of a game start or load, and the end of a cutscene unless
// DS:5FFE is set (intrinsics 0x092 / 0x094).
//
// Not reproduced: the debug freeze (11D8:3412, Scroll Lock held when the
// CRUDEBUG variable is set).

#include "engine/process.hpp"
#include "engine/world/world.hpp"

#include <array>
#include <cstdint>

namespace crusader {

class PaletteCycleProcess : public TimedProcess {
public:
    static constexpr std::uint16_t kType = 0x248;
    static constexpr std::uint32_t kRate = 30;

    explicit PaletteCycleProcess(GameState& state);

protected:
    void tick() override;

private:
    GameState* state_;
    std::array<std::array<std::uint8_t, 3>, 7> colours_;  // DS:6845
};

}  // namespace crusader
