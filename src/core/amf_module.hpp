#pragma once
// SOUND/*.AMF: the music, "ASYLUM Music Format V1.0" modules (converted from
// ProTracker MODs by MOD2AMF.EXE), loaded by amf_load (AMFLOAD.C, 12E8:00A6)
// and handed to the Asylum player (_ASS_PlayMusic).
//
// File layout, read in one go:
//   0x000  "ASYLUM Music Format V1.0", zero-padded to 0x20 (only "ASYLUM"
//          is checked: "Invalid AMF format")
//   0x020  u8 initial speed (ticks per row)
//   0x021  u8 initial tempo (BPM)
//   0x022  u8 sample count (31, or 11; not read, the player takes 31)
//   0x023  u8 pattern count
//   0x024  u8 order count
//   0x025  u8 restart order (none when not below the order count)
//   0x026  256 orders (pattern numbers)
//   0x126  64 samples of 0x25 bytes: name (22), u8 finetune (0..15),
//          u8 volume (0..64), s8 transpose (notes), u32 length, u32 loop
//          start, u32 loop length (all in bytes = samples)
//   0xA66  the patterns, 0x800 bytes each: 64 rows of 8 cells of 4 bytes
//          (note, sample, command, parameter)
//   then the signed 8-bit data of each sample with a non-zero length, in
//   order.

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace crusader {

struct AmfSample {
    std::string name;
    std::uint8_t finetune = 0;
    std::uint8_t volume = 0;
    std::int8_t transpose = 0;
    std::uint32_t length = 0;
    std::uint32_t loop_start = 0;
    std::uint32_t loop_length = 0;
    std::vector<std::int8_t> pcm;
};

struct AmfCell {
    std::uint8_t note = 0;     // 0: none; 1..: index in the period table
    std::uint8_t sample = 0;   // 0: none; 1..: sample number
    std::uint8_t command = 0;
    std::uint8_t param = 0;
};

class AmfModule {
public:
    static constexpr int kSamples = 64;
    static constexpr int kChannels = 8;
    static constexpr int kRows = 64;

    static std::optional<AmfModule> load(const std::filesystem::path& path);
    static std::optional<AmfModule> parse(std::span<const std::uint8_t> data);

    std::uint8_t speed = 6;
    std::uint8_t tempo = 125;
    std::uint8_t pattern_count = 0;
    std::uint8_t order_count = 0;
    std::uint8_t restart = 0;
    std::array<std::uint8_t, 256> orders{};
    std::array<AmfSample, kSamples> samples;

    AmfCell cell(int pattern, int row, int channel) const;

private:
    std::vector<std::uint8_t> patterns_;
};

}  // namespace crusader
