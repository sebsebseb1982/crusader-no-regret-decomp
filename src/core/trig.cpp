#include "core/trig.hpp"

#include "core/flex.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace crusader {

// @orig 12D0:0000
TrigTables TrigTables::load(const std::filesystem::path& path) {
    const auto data = read_file(path);
    if (data.size() < 2 * 256 * 4) throw std::runtime_error("TRIG.DAT too short");
    auto at = [&](std::size_t i) {
        return static_cast<std::int32_t>(data[i * 4] | data[i * 4 + 1] << 8 | data[i * 4 + 2] << 16 |
                                         static_cast<std::uint32_t>(data[i * 4 + 3]) << 24);
    };
    TrigTables t;
    for (std::size_t i = 0; i < 256; ++i) {
        t.sine[i] = at(i);
        t.cosine[i] = at(256 + i);
    }
    return t;
}

// @orig none: tables computed when TRIG.DAT is absent
TrigTables TrigTables::computed() {
    TrigTables t;
    for (std::size_t i = 0; i < 256; ++i) {
        const double a = static_cast<double>(i) * 2 * std::numbers::pi / 256;
        t.sine[i] = static_cast<std::int32_t>(std::lround(std::sin(a) * 65536));
        t.cosine[i] = static_cast<std::int32_t>(std::lround(std::cos(a) * 65536));
    }
    return t;
}

int TrigTables::index(int degrees) {
    int d = degrees % 360;
    if (d < 0) d += 360;
    return std::min(d * 1000 / 1406, 255);
}

}  // namespace crusader
