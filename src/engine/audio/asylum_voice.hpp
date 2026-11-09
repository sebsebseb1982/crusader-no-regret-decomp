#pragma once
// One channel of the Asylum mixer (0x41 bytes at DS:0928 + 0x41 * n in
// ASYLUM.DLL: 8 music channels then 12 sound effect channels), as mixed by
// 1000:0166 (16-bit cards):
//   - the position advances by `step` (16.16 fixed point) per output sample;
//   - with a loop longer than 2 it goes back by the loop length on reaching
//     loop start + loop length (the length is not checked); otherwise the
//     channel stops at its length;
//   - the signed sample s adds s * v / 2 at volume v (0..128), the table at
//     DS:3FFE (built by 1018:0360) rounding negative values towards s + 1.

#include <cstddef>
#include <cstdint>
#include <span>

namespace crusader {

struct AsylumVoice {
    std::span<const std::int8_t> pcm;
    std::uint32_t length = 0;  // +08
    std::uint32_t loop_start = 0;  // +0C
    std::uint32_t loop_length = 0;  // +10
    std::uint32_t pos = 0;   // +04, whole samples
    std::uint32_t frac = 0;  // 1/65536 sample
    std::uint32_t step = 0;  // +19
    int volume = 0;          // +14, 0..64 (0x41: full scale, sound effects)
    bool active = false;     // +35

    static int scaled(std::int8_t s, int v) {
        if (s >= 0) return s * v >> 1;
        return -((-1 - s) * v >> 1);
    }

    // Adds `count` samples at volume v (0..128) to `sum`. Returns false when
    // the channel reached its end (and stopped).
    // @orig ASYLUM.DLL:1000:0166
    bool mix(std::int32_t* sum, std::size_t count, int v) {
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
            if (pos < pcm.size()) sum[i] += scaled(pcm[pos], v);
            const std::uint32_t f = frac + step;
            pos += f >> 16;
            frac = f & 0xFFFF;
        }
        return true;
    }
};

}  // namespace crusader
