#pragma once
// SOUND/SOUND.FLX: the sound effects, opened by dsfx_init (DSFXMAN.C) as a
// "LockTagFlex" (TAGFLEX.C) and read through the sound cache (SNDCACHE.C).
//
// Entry 0 is the tag table: one record per entry 1..n, a u32 (a DOS date and
// time) then the zero-terminated name ("FOOT1", "ALARM1B"...). Sound effect
// number n is entry n.
//
// Each sound is an "ASFX" block handed as is to _ASS_PlaySFX (ASYLUM.DLL
// 1010:0024); no compression in this version:
//   0x00  "ASFX"
//   0x04  u32 length (bytes = samples)
//   0x08  u32 loop start
//   0x0C  u32 loop length (a loop needs more than 2)
//   0x10  u16 not read
//   0x12  u16 sample rate (11025, 11111 or 22050)
//   0x14  signed 8-bit mono samples
// Some entries carry bytes past the length; they are never played.

#include "core/flex.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace crusader {

struct SoundSample {
    std::span<const std::int8_t> pcm;
    std::uint32_t loop_start = 0;
    std::uint32_t loop_length = 0;
    std::uint16_t rate = 0;
};

class SoundFlex {
public:
    static SoundFlex open(const std::filesystem::path& path);

    // Sound effect `sfx`, nullopt when the entry is empty or not an ASFX.
    std::optional<SoundSample> sample(int sfx) const;
    // Its tag ("" when unknown).
    std::string name(int sfx) const;
    std::size_t count() const { return flex_.count(); }

private:
    FlexArchive flex_;
    std::vector<std::string> names_;  // names_[n - 1] for entry n
};

}  // namespace crusader
