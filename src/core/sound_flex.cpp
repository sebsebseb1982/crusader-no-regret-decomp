#include "core/sound_flex.hpp"

#include "core/bytes.hpp"

#include <algorithm>
#include <cstring>

namespace crusader {

// @orig 1258:0000 1258:026C
SoundFlex SoundFlex::open(const std::filesystem::path& path) {
    SoundFlex s;
    s.flex_ = FlexArchive::open(path);
    const auto tags = s.flex_.entry(0);
    for (std::size_t pos = 4; pos < tags.size(); pos += 4) {
        const auto end = std::find(tags.begin() + static_cast<std::ptrdiff_t>(pos), tags.end(), 0);
        s.names_.emplace_back(tags.begin() + static_cast<std::ptrdiff_t>(pos), end);
        pos = static_cast<std::size_t>(end - tags.begin()) + 1;
    }
    return s;
}

// @orig 12F8:052D
std::optional<SoundSample> SoundFlex::sample(int sfx) const {
    if (sfx <= 0) return std::nullopt;
    const auto e = flex_.entry(static_cast<std::size_t>(sfx));
    constexpr std::size_t kHeader = 0x14;
    if (e.size() < kHeader || std::memcmp(e.data(), "ASFX", 4) != 0) return std::nullopt;
    const std::uint32_t length = std::min<std::uint32_t>(read_u32(e, 4), static_cast<std::uint32_t>(e.size() - kHeader));
    SoundSample s;
    s.pcm = {reinterpret_cast<const std::int8_t*>(e.data() + kHeader), length};
    s.loop_start = read_u32(e, 8);
    s.loop_length = read_u32(e, 0xC);
    s.rate = read_u16(e, 0x12);
    return s;
}

std::string SoundFlex::name(int sfx) const {
    if (sfx <= 0 || static_cast<std::size_t>(sfx) > names_.size()) return {};
    return names_[static_cast<std::size_t>(sfx) - 1];
}

}  // namespace crusader
