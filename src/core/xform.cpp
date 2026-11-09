#include "core/xform.hpp"

#include "core/flex.hpp"

#include <algorithm>
#include <stdexcept>

namespace crusader {

XformTable XformTable::load(const std::filesystem::path& xformpal_dat) {
    const FlexArchive flex = FlexArchive::open(xformpal_dat);
    return parse(flex.entry(0));
}

// @orig 1030:0250 1030:0348
XformTable XformTable::parse(std::span<const std::uint8_t> entry0) {
    if (entry0.size() < 256 + kSize) {
        throw std::runtime_error("XFORMPAL.DAT entry 0 too short");
    }
    XformTable x;
    std::copy_n(entry0.begin() + 256, kSize, x.t_.begin());
    // The blitter indexes up to table 7; reject anything that would read past it.
    for (int c = 0; c < 256; ++c) {
        const std::uint8_t d = x.t_[c];
        if (d != 0xFF && (d == 0 || d > 7)) {
            throw std::runtime_error("XFORMPAL.DAT: bad blend table number");
        }
    }
    return x;
}

}  // namespace crusader
