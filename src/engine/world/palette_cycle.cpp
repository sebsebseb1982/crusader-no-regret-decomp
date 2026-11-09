#include "engine/world/palette_cycle.hpp"

#include "engine/usecode/world_intrinsics.hpp"

#include <algorithm>

namespace crusader {

namespace {

constexpr int kColours = 7;
constexpr std::array<int, kColours> kEntries = {8, 9, 10, 11, 12, 13, 14};  // DS:6814
constexpr std::array<int, kColours> kModes = {0, 0, 0, 0, 0, 0, 1};         // DS:6822
constexpr std::array<std::array<bool, 3>, kColours> kRising = {{            // DS:6830
    {true, false, false},
    {false, false, true},
    {true, false, false},
    {false, false, true},
    {true, true, false},
    {true, true, true},
    {false, true, false},
}};

}  // namespace

PaletteCycleProcess::PaletteCycleProcess(GameState& state)
    : state_(&state),
      colours_{{{0, 0, 0}, {0, 0, 0}, {0x1F, 0, 0}, {0, 0, 0x1F}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}} {
    set_type(kType);
    set_rate(kRate);
    set_runs_at_any_level(true);
}

// @orig 1438:011B
void PaletteCycleProcess::tick() {
    if (state_->cycle_paused) return;
    std::array<std::uint8_t, 768> game = state_->game_palette.to_vga();
    // Entries 1..7 rotate down.
    std::array<std::uint8_t, 3> first;
    std::copy_n(game.begin() + 3, 3, first.begin());
    std::copy(game.begin() + 6, game.begin() + 24, game.begin() + 3);
    std::copy(first.begin(), first.end(), game.begin() + 21);
    for (int i = 0; i < kColours; ++i) {
        if (kModes[i] > 1) continue;
        std::array<std::uint8_t, 3>& c = colours_[i];
        // UNITFILE_1438_0395: the flagged components rise by 2, wrapping to 0.
        bool wrapped = false;
        for (int k = 0; k < 3; ++k) {
            if (kRising[i][k]) c[k] = static_cast<std::uint8_t>(c[k] + 2);
            if (c[k] > 63) {
                c[k] = 0;
                wrapped = true;
            }
        }
        if (kModes[i] == 1 && wrapped) {
            for (std::uint8_t& v : c) v = static_cast<std::uint8_t>((v + random_range(state_->random_state, 10)) & 0x3F);
        }
        std::copy(c.begin(), c.end(), game.begin() + 3 * kEntries[i]);
    }
    state_->game_palette = Palette::from_vga(game);
    // Entries 1..14 to the DAC.
    std::array<std::uint8_t, 768> dac = state_->screen_palette.to_vga();
    std::copy(game.begin() + 3, game.begin() + 45, dac.begin() + 3);
    state_->screen_palette = Palette::from_vga(dac);
}

}  // namespace crusader
