#include "core/typeflags.hpp"

#include "core/flex.hpp"

#include <algorithm>

namespace crusader {

// @orig ?: TYPEFLAG.DAT reader
ShapeInfo ShapeInfo::decode(const std::uint8_t* d) {
    ShapeInfo s;
    s.flags = d[0] | (d[1] & 0x0Fu) << 8 | std::uint32_t{d[6]} << 16;
    s.family = static_cast<std::uint8_t>(d[1] >> 4 | (d[2] & 1) << 4);
    s.equip_type = static_cast<std::uint8_t>((d[2] >> 1) & 0x0F);
    s.footpad_x = static_cast<std::uint8_t>((d[3] << 3 | d[2] >> 5) & 0x1F);
    s.footpad_y = static_cast<std::uint8_t>((d[3] >> 2) & 0x1F);
    s.footpad_z = static_cast<std::uint8_t>((d[4] << 1 | d[3] >> 7) & 0x1F);
    s.anim_type = static_cast<std::uint8_t>(d[4] >> 4);
    s.anim_data = static_cast<std::uint8_t>(d[5] & 0x0F);
    s.anim_speed = static_cast<std::uint8_t>(d[5] >> 4);
    s.weight = d[7];
    s.volume = d[8];
    std::copy(d, d + s.bytes.size(), s.bytes.begin());
    return s;
}

// @orig ?: TYPEFLAG.DAT reader
std::vector<ShapeInfo> load_typeflags(const std::filesystem::path& path) {
    constexpr std::size_t kRecord = 9;
    const auto data = read_file(path);
    std::vector<ShapeInfo> infos;
    infos.reserve(data.size() / kRecord);
    for (std::size_t pos = 0; pos + kRecord <= data.size(); pos += kRecord) {
        infos.push_back(ShapeInfo::decode(&data[pos]));
    }
    return infos;
}

}  // namespace crusader
