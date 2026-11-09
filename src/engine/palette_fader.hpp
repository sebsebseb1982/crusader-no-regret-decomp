#pragma once
// Palette fades (FADE.C, segment 1440): "FadeProcess", type 0x206.
//
// FADE_1440_0000 (this, target, rate, flag, count, step): with a rate of 0 the
// target palette is written to the DAC at once. Otherwise a timed process at
// `rate` Hz starts from the palette on screen (palette_read_hw) and, at each
// tick (FADE_1440_03a0), moves each of the 768 6-bit DAC components towards
// the target: by `step` when farther than that, onto the target otherwise.
// It then decrements `count`; the fade ends when every component already
// equalled the target before the tick or when `count` reaches 0, and in both
// cases that last tick writes nothing. Otherwise the new palette is written.
// A count of 0 therefore never ends the fade early (it goes negative).
//
// Usecode intrinsics (arguments in call order, see world_intrinsics.cpp):
//   0x078 fadeToBlack()               black, 40 Hz
//   0x09E fadeToBlack(rate)           black
//   0x09F fadeToBlack(rate, count)
//   0x07B fadeFromBlack()             game palette, 40 Hz
//   0x09C fadeToGamePal(rate)         game palette
//   0x09D fadeToGamePal(rate, count)
//   0x0A0 fadeToGivenColor(r, g, b, rate, count)   r, g, b in 0..63
//   0x090 jumpToAllBlack, 0x0DA jumpToAllWhite (63, 63, 63),
//   0x09B jumpToNormalPalette (game palette)
//   0x098 jumpToGreyScale (luminance of the game palette)
// The step is always 1. The fades return the fade process's pid; 0x09C and
// 0x07B return instead a second process that waits for the fade (vtable
// 68A7, FadeWaitProcess).
//
// Colour cycling (palette_cycle.hpp): a fade with a rate keeps whether the
// cycling was paused (+0x40) and pauses it while it runs. When it ends or
// fails (FADE_1440_0278), the cycling resumes if it was running before or if
// the fade's flag +0x41 is set: fadeToGamePal(rate, count) 0x09D,
// fadeToBlack(rate, count) 0x09F and fadeToGivenColor 0x0A0. The other fades
// to black and the greyscale fade pause the cycling first, so it stays
// paused; the jumps to black, white and grey pause it, jumpToNormalPalette
// resumes it, and so does the waiting process of fadeToGamePal (1440:0F2F).

#include "core/palette.hpp"
#include "engine/process.hpp"

#include <array>
#include <cstdint>

namespace crusader {

// palette_rgb_to_yiq: Y, from 6-bit components.
std::uint8_t luminance(std::uint8_t r, std::uint8_t g, std::uint8_t b);
// Every entry to its luminance (FADE_1440_05BE, jumpToGreyScale).
Palette greyscale(const Palette& p);

// The fade itself: at each tick (FADE_1440_03a0), every component moves
// towards the target by `step`. tick() returns false when the fade ended
// (all components equal, or the count reached 0), the palette unchanged.
class PaletteFade {
public:
    PaletteFade(const Palette& from, const Palette& target, int count, int step = 1);
    bool tick(Palette& out);

private:
    std::array<std::uint8_t, 768> current_;
    std::array<std::uint8_t, 768> target_;
    int count_;
    int step_;
};

class PaletteFaderProcess : public TimedProcess {
public:
    static constexpr std::uint16_t kType = 0x206;
    static constexpr std::uint32_t kDefaultRate = 40;

    // `screen` is the DAC contents the fade reads and writes. `cycle_paused`
    // (may be null: no colour cycling) is GameState::cycle_paused;
    // `resume_cycle` is the flag +0x41.
    PaletteFaderProcess(Palette& screen, const Palette& target, std::uint32_t rate, int count, int step = 1,
                        bool* cycle_paused = nullptr, bool resume_cycle = false);

protected:
    void tick() override;
    void on_added() override;
    void on_terminate() override;

private:
    Palette* screen_;
    Palette target_;
    PaletteFade fade_;
    std::uint32_t rate_;
    bool* cycle_paused_;
    bool resume_cycle_;
    bool was_paused_ = true;  // +0x40
};

// The process returned by fadeToGamePal and fadeFromBlack (vtable 68A7): it
// waits for the fade, then resumes the colour cycling and ends (1440:0F2F).
class FadeWaitProcess : public Process {
public:
    FadeWaitProcess(Pid fade, bool& cycle_paused);

protected:
    void run() override;
    void on_added() override;

private:
    Pid fade_;
    bool* cycle_paused_;
};

}  // namespace crusader
