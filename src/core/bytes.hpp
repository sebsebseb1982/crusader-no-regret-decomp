#pragma once
// Little-endian reads with bounds checks.

#include <cstdint>
#include <span>
#include <stdexcept>

namespace crusader {

inline void check_range(std::span<const std::uint8_t> d, std::size_t pos, std::size_t len) {
    if (pos > d.size() || len > d.size() - pos) {
        throw std::out_of_range("read past end of data");
    }
}

inline std::uint8_t read_u8(std::span<const std::uint8_t> d, std::size_t pos) {
    check_range(d, pos, 1);
    return d[pos];
}

inline std::uint16_t read_u16(std::span<const std::uint8_t> d, std::size_t pos) {
    check_range(d, pos, 2);
    return static_cast<std::uint16_t>(d[pos] | d[pos + 1] << 8);
}

inline std::uint32_t read_u24(std::span<const std::uint8_t> d, std::size_t pos) {
    check_range(d, pos, 3);
    return d[pos] | d[pos + 1] << 8 | static_cast<std::uint32_t>(d[pos + 2]) << 16;
}

inline std::uint32_t read_u32(std::span<const std::uint8_t> d, std::size_t pos) {
    check_range(d, pos, 4);
    return d[pos] | d[pos + 1] << 8 | d[pos + 2] << 16 | static_cast<std::uint32_t>(d[pos + 3]) << 24;
}

inline std::int32_t read_i32(std::span<const std::uint8_t> d, std::size_t pos) {
    return static_cast<std::int32_t>(read_u32(d, pos));
}

}  // namespace crusader
