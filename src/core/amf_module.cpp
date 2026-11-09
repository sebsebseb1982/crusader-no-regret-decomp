#include "core/amf_module.hpp"

#include "core/bytes.hpp"
#include "core/flex.hpp"

#include <algorithm>
#include <cstring>

namespace crusader {

namespace {

constexpr std::size_t kSampleTable = 0x126;
constexpr std::size_t kSampleSize = 0x25;
constexpr std::size_t kHeader = 0xA66;
constexpr std::size_t kPatternSize = 0x800;

}  // namespace

std::optional<AmfModule> AmfModule::load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return std::nullopt;
    const auto data = read_file(path);
    return parse(data);
}

// @orig 12E8:00A6
std::optional<AmfModule> AmfModule::parse(std::span<const std::uint8_t> d) {
    if (d.size() < kHeader || std::memcmp(d.data(), "ASYLUM", 6) != 0) return std::nullopt;
    AmfModule m;
    m.speed = d[0x20];
    m.tempo = d[0x21];
    m.pattern_count = d[0x23];
    m.order_count = d[0x24];
    m.restart = d[0x25];
    std::copy_n(d.begin() + 0x26, m.orders.size(), m.orders.begin());
    std::size_t pos = kHeader + m.pattern_count * kPatternSize;
    if (pos > d.size()) return std::nullopt;
    m.patterns_.assign(d.begin() + kHeader, d.begin() + static_cast<std::ptrdiff_t>(pos));
    for (int i = 0; i < kSamples; ++i) {
        const std::size_t at = kSampleTable + static_cast<std::size_t>(i) * kSampleSize;
        AmfSample& s = m.samples[static_cast<std::size_t>(i)];
        const auto name = d.subspan(at, 22);
        s.name.assign(name.begin(), std::find(name.begin(), name.end(), 0));
        s.finetune = d[at + 22];
        s.volume = d[at + 23];
        s.transpose = static_cast<std::int8_t>(d[at + 24]);
        s.length = read_u32(d, at + 25);
        s.loop_start = read_u32(d, at + 29);
        s.loop_length = read_u32(d, at + 33);
        if (s.length == 0) continue;
        if (s.length > d.size() - pos) return std::nullopt;
        const auto* pcm = reinterpret_cast<const std::int8_t*>(d.data() + pos);
        s.pcm.assign(pcm, pcm + s.length);
        pos += s.length;
    }
    return m;
}

// @orig ?: cell decoding inside the ASYLUM.DLL player
AmfCell AmfModule::cell(int pattern, int row, int channel) const {
    const std::size_t at = (static_cast<std::size_t>(pattern) * kRows * kChannels +
                            static_cast<std::size_t>(row) * kChannels + static_cast<std::size_t>(channel)) * 4;
    if (at + 4 > patterns_.size()) return {};
    return {patterns_[at], patterns_[at + 1], patterns_[at + 2], patterns_[at + 3]};
}

}  // namespace crusader
