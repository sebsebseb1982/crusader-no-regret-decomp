#include "engine/usecode/usecode_file.hpp"

#include "core/bytes.hpp"

#include <algorithm>
#include <cstdio>

namespace crusader {

namespace {
constexpr std::size_t kEventTable = 20;
constexpr std::size_t kEventSize = 6;
constexpr std::size_t kNameSize = 13;
}  // namespace

// @orig ?: class loading of UPROCESS.C (segment 1420), not split yet
UsecodeFile UsecodeFile::from_flex(const FlexArchive& flex) {
    UsecodeFile uc;
    const std::size_t count = flex.count() >= 2 ? flex.count() - 2 : 0;
    uc.classes_.resize(count);
    uc.names_.resize(count);
    const auto names = flex.entry(1);
    for (std::size_t cid = 0; cid < count; ++cid) {
        const auto blob = flex.entry(cid + 2);
        uc.classes_[cid].assign(blob.begin(), blob.end());
        const std::size_t pos = 4 + kNameSize * cid;
        for (std::size_t i = pos; i < pos + kNameSize && i < names.size() && names[i]; ++i) {
            uc.names_[cid] += static_cast<char>(names[i]);
        }
    }
    return uc;
}

// @orig none: classes built by the tests
UsecodeFile UsecodeFile::from_classes(std::vector<std::vector<std::uint8_t>> classes) {
    UsecodeFile uc;
    uc.classes_ = std::move(classes);
    uc.names_.resize(uc.classes_.size());
    return uc;
}

// @orig none: class name for the tools
std::string UsecodeFile::class_name(std::uint16_t cid) const {
    if (cid < names_.size() && !names_[cid].empty()) {
        return names_[cid];
    }
    char buf[16];
    std::snprintf(buf, sizeof buf, "class_%04X", cid);
    return buf;
}

std::size_t UsecodeFile::base(std::uint16_t cid) const {
    const auto& c = classes_[cid];
    const std::size_t b = read_u32(c, 8);
    return b == 0 ? c.size() : b - 1;
}

// @orig ?: class header reading of UPROCESS.C (segment 1420), not split yet
std::span<const std::uint8_t> UsecodeFile::code(std::uint16_t cid) const {
    if (!has_class(cid)) {
        return {};
    }
    const std::size_t b = base(cid);
    return b < classes_[cid].size() ? std::span(classes_[cid]).subspan(b) : std::span<const std::uint8_t>{};
}

// @orig ?: class header reading of UPROCESS.C (segment 1420), not split yet
std::size_t UsecodeFile::event_count(std::uint16_t cid) const {
    if (!has_class(cid)) {
        return 0;
    }
    // The table ends exactly one byte past `base`: the "pad" byte at base is the
    // high byte of the last entry's offset (verified on all 402 classes).
    const std::size_t end = std::min(base(cid) + 1, classes_[cid].size());
    return end > kEventTable ? (end - kEventTable) / kEventSize : 0;
}

// @orig ?: class header reading of UPROCESS.C (segment 1420), not split yet
std::uint32_t UsecodeFile::event_offset(std::uint16_t cid, std::uint32_t n) const {
    if (n >= event_count(cid)) {
        return 0;
    }
    return read_u32(classes_[cid], kEventTable + kEventSize * n + 2);
}

// @orig none: classes built by the tests
std::vector<std::uint8_t> UsecodeFile::make_class(const std::vector<std::uint32_t>& event_offsets,
                                                  const std::vector<std::uint8_t>& code_after_pad) {
    const std::size_t base = kEventTable + kEventSize * event_offsets.size();
    std::vector<std::uint8_t> c(base, 0);
    auto put32 = [&](std::size_t pos, std::uint32_t v) {
        for (int i = 0; i < 4; ++i) c[pos + i] = static_cast<std::uint8_t>(v >> (8 * i));
    };
    put32(8, static_cast<std::uint32_t>(base + 1));
    for (std::size_t i = 0; i < event_offsets.size(); ++i) {
        put32(kEventTable + kEventSize * i + 2, event_offsets[i]);
    }
    c.push_back(0);  // pad byte at base: code offsets start at 1
    c.insert(c.end(), code_after_pad.begin(), code_after_pad.end());
    return c;
}

}  // namespace crusader
