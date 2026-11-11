#pragma once
// One channel of the Asylum mixer: 8 music channels then 12 sound effect
// channels (0x41 bytes at DS:0928 + 0x41 * n in No Remorse's ASYLUM.DLL,
// 0x46 bytes at DS:0738 + 0x46 * n in No Regret's), as mixed by the
// routines of segment 1000:
//   - the position advances by `step` (16.16 fixed point) per output frame;
//   - with a loop longer than 2 it goes back by the loop length on reaching
//     loop start + loop length (the length is not checked); otherwise the
//     channel stops at its length;
//   - the signed sample s adds T(s, v) at level v (0..64, or 0x41: full
//     scale), from the table at DS:4524 (built by 1018:036D in No Regret,
//     1018:0360 in No Remorse):
//       16-bit outputs: s * v / 2, negative values rounded towards s + 1
//       (0x41: s * 64, (s + 1) * 64);
//       8-bit outputs (No Regret only): s * v / 128, negative values
//       rounded the same way (0x41: s itself, both signs).
// No Remorse mixes into one output (1000:0166). No Regret's channel holds
// the routine that mixes it (+44) and the shares of its two outputs (+15
// the second, +16 the first; a share scales the level: level * share / 64):
//   - 1000:068C (16-bit) / 017E (8-bit): each output at its share;
//   - 1000:08A4 (16-bit) / 0386 (8-bit), "surround" (E80 in the music): the
//     sample on both outputs in antiphase, T on one and -T - 1 on the other
//     (the first output inverted in 16 bits, the second in 8 bits).
// The 16-bit routines add both outputs as one 32-bit word: a carry out of
// the first reaches the second (one step in 65536, not reproduced).

#include <cstddef>
#include <cstdint>
#include <span>

namespace crusader {

// The sample size of the card's output (DS:0105).
enum class SampleDepth { k8, k16 };

struct AsylumVoice {
    static constexpr int kFullScale = 0x41;
    static constexpr int kFullShare = 0x40;

    std::span<const std::int8_t> pcm;
    std::uint32_t length = 0;  // +08
    std::uint32_t loop_start = 0;  // +0C
    std::uint32_t loop_length = 0;  // +10
    std::uint32_t pos = 0;   // +04, whole samples
    std::uint32_t frac = 0;  // +17 in No Regret, 1/65536 sample
    std::uint32_t step = 0;  // +1B in No Regret (+19 in No Remorse)
    int volume = 0;          // +14, 0..64 (0x41: full scale, sound effects)
    bool active = false;     // +37 in No Regret (+35 in No Remorse)
    int first_share = kFullShare;   // +16
    int second_share = kFullShare;  // +15
    bool surround = false;  // +44 is 1000:08A4 / 0386

    // @orig ASYLUM.DLL:1018:036D
    static int scaled(std::int8_t s, int v, SampleDepth depth) {
        if (depth == SampleDepth::k8) {
            if (v == kFullScale) return s;
            if (s >= 0) return s * v >> 7;
            return -((-1 - s) * v >> 7);
        }
        if (v == kFullScale) v = 2 * kFullShare;
        if (s >= 0) return s * v >> 1;
        return -((-1 - s) * v >> 1);
    }

    // Adds `count` samples at level v to the mono `sum` (No Remorse
    // 1000:0166, 16 bits). Returns false when the channel reached its end
    // (and stopped).
    // @orig none: No Remorse ASYLUM.DLL 1000:0166, which No Regret's DLL keeps as 1000:050A for its mono cards
    bool mix(std::int32_t* sum, std::size_t count, int v) {
        return run(count, [&](std::size_t i, std::int8_t s) { sum[i] += scaled(s, v, SampleDepth::k16); });
    }

    // Adds `frames` frames to the stereo `sum` (first, second, first...):
    // T(s, first) to the first output, T(s, second) to the second.
    // @orig ASYLUM.DLL:1000:068C ASYLUM.DLL:1000:017E
    bool mix_stereo(std::int32_t* sum, std::size_t frames, int first, int second, SampleDepth depth) {
        return run(frames, [&](std::size_t i, std::int8_t s) {
            sum[2 * i] += scaled(s, first, depth);
            sum[2 * i + 1] += scaled(s, second, depth);
        });
    }

    // The surround routines: T(s, v) in antiphase.
    // @orig ASYLUM.DLL:1000:08A4 ASYLUM.DLL:1000:0386
    bool mix_surround(std::int32_t* sum, std::size_t frames, int v, SampleDepth depth) {
        return run(frames, [&](std::size_t i, std::int8_t s) {
            const int t = scaled(s, v, depth);
            const bool first_inverted = depth == SampleDepth::k16;
            sum[2 * i] += first_inverted ? -t - 1 : t;
            sum[2 * i + 1] += first_inverted ? t : -t - 1;
        });
    }

private:
    // The loop shared by the routines.
    // @orig ASYLUM.DLL:1000:050A
    template <class Add>
    bool run(std::size_t count, Add add) {
        if (!active || length == 0) return true;
        const bool loops = loop_length > 2;
        const std::uint32_t loop_end = loop_start + loop_length;
        for (std::size_t i = 0; i < count; ++i) {
            if (loops) {
                if (pos >= loop_end) pos -= loop_length;
            } else if (pos >= length) {
                active = false;
                return false;
            }
            if (pos < pcm.size()) add(i, pcm[pos]);
            const std::uint32_t f = frac + step;
            pos += f >> 16;
            frac = f & 0xFFFF;
        }
        return true;
    }
};

}  // namespace crusader
