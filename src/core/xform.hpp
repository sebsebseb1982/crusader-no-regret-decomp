#pragma once
// Translucency tables (STATIC/XFORMPAL.DAT), loaded by GAMEPAL.C (1030:0250)
// and used by the translucent asm blitter (asm_blit_4, copied from 1000:0F88).
//
// XFORMPAL.DAT is a Flex. Entry 0 (2304 bytes) is read whole; the blitter is
// handed a pointer 256 bytes into it, call it T:
//   T[0..255]          colour -> blend table number d (1..7), or 0xFF = opaque
//   T[d*256 + dst]     the colour that replaces `dst` under translucent colour d
// Only palette colours 8..14 are translucent (tables 1..7). The first 256 bytes
// of the entry are not read by the blitter. Entry 1 (32 bytes) is not used for
// drawing.

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>

namespace crusader {

class XformTable {
public:
    static XformTable load(const std::filesystem::path& xformpal_dat);
    static XformTable parse(std::span<const std::uint8_t> entry0);

    // The pixel the translucent blitter writes for source colour `src` over `dst`.
    std::uint8_t blend(std::uint8_t src, std::uint8_t dst) const {
        const std::uint8_t d = t_[src];
        return d == 0xFF ? src : t_[d * 256 + dst];
    }

    bool translucent(std::uint8_t src) const { return t_[src] != 0xFF; }

private:
    static constexpr std::size_t kSize = 2304 - 256;
    std::array<std::uint8_t, kSize> t_{};
};

}  // namespace crusader
