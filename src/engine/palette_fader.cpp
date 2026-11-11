#include "engine/palette_fader.hpp"

#include "core/language.hpp"
#include "engine/kernel.hpp"

namespace crusader {

std::uint8_t luminance(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    return static_cast<std::uint8_t>((r * 299u + g * 587u + b * 114u) / 1000u);
}

// @orig 1418:05F3
Palette greyscale(const Palette& p) {
    std::array<std::uint8_t, 768> vga = p.to_vga();
    for (std::size_t i = 0; i < 256; ++i) {
        const std::uint8_t y = luminance(vga[3 * i], vga[3 * i + 1], vga[3 * i + 2]);
        vga[3 * i] = vga[3 * i + 1] = vga[3 * i + 2] = y;
    }
    return Palette::from_vga(vga);
}

PaletteFade::PaletteFade(const Palette& from, const Palette& target, int count, int step)
    : current_(from.to_vga()), target_(target.to_vga()), count_(count), step_(step) {}

// @orig 1418:03D5
bool PaletteFade::tick(Palette& out) {
    int equal = 0;
    for (std::size_t i = 0; i < current_.size(); ++i) {
        const int cur = current_[i];
        const int tgt = target_[i];
        if (cur == tgt) {
            ++equal;
        } else if (cur + step_ < tgt) {
            current_[i] = static_cast<std::uint8_t>(cur + step_);
        } else if (tgt < cur - step_) {
            current_[i] = static_cast<std::uint8_t>(cur - step_);
        } else {
            current_[i] = static_cast<std::uint8_t>(tgt);
        }
    }
    --count_;
    if (equal == static_cast<int>(current_.size()) || count_ == 0) return false;
    out = Palette::from_vga(current_);
    return true;
}

// @orig 1418:0000
PaletteFaderProcess::PaletteFaderProcess(Palette& screen, const Palette& target, std::uint32_t rate, int count,
                                         int step, bool* cycle_paused, bool resume_cycle)
    : screen_(&screen),
      target_(Palette::from_vga(target.to_vga())),
      fade_(screen, target, count, step),
      rate_(rate),
      cycle_paused_(cycle_paused),
      resume_cycle_(resume_cycle) {
    set_type(kType);
    if (rate_ != 0) {
        set_rate(rate_);
        // FADE_1440_0000: the cycling pauses while the fade runs.
        if (cycle_paused_) {
            was_paused_ = *cycle_paused_;
            *cycle_paused_ = true;
        }
    }
}

// @orig 1418:0000
// No Regret first looks for another fade (11E8:13A4, type 0x206): when one
// runs, the new process is never activated: no rate,
// no palettes, flag +0x41 cleared, the colour cycling not paused. It does
// nothing and does not end by itself (a kill of the type 0x206 ends it).
void PaletteFaderProcess::on_added() {
    if (rate_ == 0) {
        *screen_ = target_;
        terminate();
        return;
    }
    if (regret_data() && kernel().count_processes(0, kType) > 1) {
        inert_ = true;
        resume_cycle_ = false;
        if (cycle_paused_) *cycle_paused_ = was_paused_;
    }
}

void PaletteFaderProcess::tick() {
    if (inert_) return;
    if (!fade_.tick(*screen_)) terminate();
}

// FADE_1440_0278.
void PaletteFaderProcess::on_terminate() {
    if (rate_ != 0 && cycle_paused_ && (resume_cycle_ || !was_paused_)) *cycle_paused_ = false;
}

FadeWaitProcess::FadeWaitProcess(Pid fade, bool& cycle_paused) : fade_(fade), cycle_paused_(&cycle_paused) {}

void FadeWaitProcess::on_added() { wait_for(fade_); }

void FadeWaitProcess::run() {
    *cycle_paused_ = false;
    terminate();
}

}  // namespace crusader
