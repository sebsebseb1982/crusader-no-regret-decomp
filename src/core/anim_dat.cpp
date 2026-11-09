#include "core/anim_dat.hpp"

#include "core/bytes.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace crusader {

// @orig 1180:0013
AnimFrame AnimFrame::decode(std::span<const std::uint8_t, 8> d) {
    AnimFrame f;
    std::copy(d.begin(), d.end(), f.raw.begin());
    const std::uint16_t w0 = static_cast<std::uint16_t>(d[0] | d[1] << 8);
    const std::uint16_t w4 = static_cast<std::uint16_t>(d[4] | d[5] << 8);
    f.frame = w0 & 0x0FFF;
    f.special = w0 & 0x1000;
    f.event = w0 & 0x4000;
    f.mirrored = w0 & 0x8000;
    f.delta_z = static_cast<std::int8_t>(d[2]);
    f.sfx = d[3];
    // 9-bit signed distance.
    f.distance = static_cast<std::int16_t>((w4 & 0x1FF) ^ 0x100) - 0x100;
    f.ground_check = w4 & 0x200;
    // Arithmetic shifts on 16-bit words, as the original tests them.
    const auto w56 = static_cast<std::int16_t>(d[5] | d[6] << 8);
    f.muzzle_a = static_cast<std::int8_t>(static_cast<std::int16_t>(w56 << 5) >> 11);
    f.muzzle_b = static_cast<std::int8_t>(static_cast<std::int16_t>(d[6] << 8) >> 11);
    f.muzzle_z = d[7] & 0x0F;
    return f;
}

// @orig 1168:0106
const AnimFrame* AnimAction::frame(int dir16, std::size_t index) const {
    int dir = dir16 & 0xFF;
    if (!sixteen_directions()) dir >>= 1;
    if (dir < 0 || static_cast<std::size_t>(dir) >= directions() || index >= frame_count()) return nullptr;
    return &frames[static_cast<std::size_t>(dir) * frame_count() + index];
}

// @orig none: whole file read at once
AnimDat AnimDat::load(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path.string());
    const std::vector<std::uint8_t> data{std::istreambuf_iterator<char>(in), {}};
    return parse(data);
}

// @orig 1170:0615 1168:0041
AnimDat AnimDat::parse(std::span<const std::uint8_t> data) {
    AnimDat dat;
    for (std::size_t shape = 0; shape < kShapes; ++shape) {
        const std::uint32_t table = read_u32(data, 4 * shape);
        if (table == 0) continue;
        auto& actions = dat.shapes_[shape];
        actions.resize(kActions);
        for (std::size_t a = 0; a < kActions; ++a) {
            const std::uint32_t at = read_u32(data, table + 4 * a);
            if (at == 0) continue;
            AnimAction action;
            for (std::size_t i = 0; i < 4; ++i) action.header[i] = read_u8(data, at + i);
            const std::size_t count = action.frame_count() * action.directions();
            check_range(data, at + 4, count * 8);
            action.frames.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                action.frames.push_back(AnimFrame::decode(data.subspan(at + 4 + 8 * i).first<8>()));
            }
            actions[a] = std::move(action);
        }
    }
    return dat;
}

const AnimAction* AnimDat::action(std::uint16_t shape, std::uint16_t action) const {
    if (shape >= kShapes || action >= kActions || shapes_[shape].empty()) return nullptr;
    const auto& a = shapes_[shape][action];
    return a ? &*a : nullptr;
}

// @orig none: count for the tools
std::size_t AnimDat::action_count() const {
    std::size_t n = 0;
    for (const auto& s : shapes_) {
        for (const auto& a : s) n += a.has_value();
    }
    return n;
}

}  // namespace crusader
