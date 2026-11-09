#pragma once
// STATIC/TRIG.DAT (read by PFLOAT_12d0_0000 at start-up): three tables of
// 256 signed 16.16 values, one entry per 360/256 degrees, kept in DS:44E2
// (sine), DS:44E6 (cosine) and DS:44EA (a second copy of the cosines). The
// sines of the file are a little below the true values (up to 206 / 65536).
//
// PFLOAT_12d0_06f7 / 07e5 look a whole number of degrees up: the angle
// modulo 360 (made positive), times 1000 / 1406 (integer division), at most
// 255.

#include <array>
#include <cstdint>
#include <filesystem>

namespace crusader {

struct TrigTables {
    std::array<std::int32_t, 256> sine{};
    std::array<std::int32_t, 256> cosine{};

    static TrigTables load(const std::filesystem::path& path);
    // Rounded true values, when the file is not there (tests).
    static TrigTables computed();

    static int index(int degrees);
    std::int32_t sin_deg(int degrees) const { return sine[static_cast<std::size_t>(index(degrees))]; }
    std::int32_t cos_deg(int degrees) const { return cosine[static_cast<std::size_t>(index(degrees))]; }
};

}  // namespace crusader
