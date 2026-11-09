#include "core/random.hpp"

#include <utility>

namespace crusader {

// @orig 12D8:04A0
std::uint16_t random_next(std::uint32_t& state) {
    // Nearly state * 65539 + 1, in 16-bit words: the low word becomes
    // 3 lo + 1 (the carry of the + 1 is lost), the high word the high word of
    // 3 state plus lo, with the carry out of that addition added back.
    const std::uint32_t lo = state & 0xFFFF;
    const std::uint32_t triple_hi = static_cast<std::uint16_t>((state * 3u) >> 16);
    const std::uint32_t sum = triple_hi + lo;
    const auto hi = static_cast<std::uint16_t>(sum + (sum >> 16));
    state = static_cast<std::uint32_t>(hi) << 16 | static_cast<std::uint16_t>(lo * 3 + 1);
    return hi;
}

// @orig 12D8:04FF
std::uint16_t random_range(std::uint32_t& state, std::uint16_t n) {
    const std::uint16_t r = random_next(state);
    return n == 0 ? 0 : static_cast<std::uint16_t>(r % n);
}

// @orig 12E0:0293
std::int16_t random_between(std::uint32_t& state, std::int16_t lo, std::int16_t hi) {
    if (lo > hi) std::swap(lo, hi);
    const auto span = static_cast<std::uint16_t>(hi - lo + 1);
    return static_cast<std::int16_t>(lo + random_range(state, span));
}

// @orig 12D8:04E3
std::uint32_t random_seed(std::uint32_t saved) { return saved | 1u; }

// @orig 1000:3775
std::uint16_t crt_rand(std::uint32_t& state) {
    state = state * 0x015A4E35u + 1u;
    return static_cast<std::uint16_t>((state >> 16) & 0x7FFF);
}

int crt_rand_below(std::uint32_t& state, int n) {
    return static_cast<int>(static_cast<std::int32_t>(crt_rand(state)) * n / 0x8000);
}

}  // namespace crusader
