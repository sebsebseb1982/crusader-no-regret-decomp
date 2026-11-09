#include "engine/usecode/movie_intrinsics.hpp"

namespace crusader {

namespace {

using Args = std::span<const std::uint8_t>;

std::uint16_t arg16(Args a, std::size_t pos) {
    return pos + 2 <= a.size() ? static_cast<std::uint16_t>(a[pos] | a[pos + 1] << 8) : 0;
}

}  // namespace

// @orig none: binds the intrinsics, numbered by USECODE/UNKCOFF.DAT
std::vector<std::uint16_t> register_movie_intrinsics(UCMachine& m, PlayMovie play) {
    std::vector<std::uint16_t> bound;
    auto bind = [&](std::initializer_list<std::uint16_t> numbers, MovieKind kind) {
        for (const std::uint16_t n : numbers) {
            m.set_intrinsic(n, [play, kind](UCMachine& machine, UCProcess&, Args a) -> std::uint32_t {
                const std::uint32_t ptr = arg16(a, 4) | std::uint32_t{arg16(a, 6)} << 16;
                if (ptr >> 16 != UCMachine::kSegString) return 0;
                const std::string* name = machine.heap().string(static_cast<std::uint16_t>(ptr));
                if (!name || !play) return 0;
                play(*name, kind);
                return 0;
            });
            bound.push_back(n);
        }
    };
    bind({0x0D2, 0x00F}, MovieKind::kCutscene);
    bind({0x093, 0x0AA}, MovieKind::kCutsceneNoFade);
    bind({0x060}, MovieKind::kOverlay);
    return bound;
}

}  // namespace crusader
