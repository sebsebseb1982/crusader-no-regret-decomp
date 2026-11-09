#pragma once
// Usecode classes (USECODE/EUSECODE.FLX), as loaded by UPROCESS.C.
//
// Flex entry 1 holds the class names (13 bytes each, from offset 4); class
// `cid` is entry cid + 2:
//   +8   u32 code base + 1
//   +20  event table, 6 bytes per entry: u16 ?, u32 offset (relative to base)
//   base 0x00 pad, then bytecode. Instruction pointers are relative to base.
// Crusader's `call`/`spawn` operands are event numbers, resolved through the
// target class's event table.

#include "core/flex.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace crusader {

class UsecodeFile {
public:
    static UsecodeFile from_flex(const FlexArchive& flex);
    // For tests: raw class blobs indexed by class id, no names.
    static UsecodeFile from_classes(std::vector<std::vector<std::uint8_t>> classes);

    std::size_t class_count() const { return classes_.size(); }
    bool has_class(std::uint16_t cid) const { return cid < classes_.size() && !classes_[cid].empty(); }
    std::string class_name(std::uint16_t cid) const;

    // Bytecode of a class, indexed by instruction pointer.
    std::span<const std::uint8_t> code(std::uint16_t cid) const;
    std::size_t event_count(std::uint16_t cid) const;
    // Code offset of event/function `n`, 0 if absent.
    std::uint32_t event_offset(std::uint16_t cid, std::uint32_t n) const;

    // Builds a class blob in the on-disk layout (tests and tools).
    static std::vector<std::uint8_t> make_class(const std::vector<std::uint32_t>& event_offsets,
                                                const std::vector<std::uint8_t>& code_after_pad);

private:
    std::size_t base(std::uint16_t cid) const;

    std::vector<std::vector<std::uint8_t>> classes_;
    std::vector<std::string> names_;
};

}  // namespace crusader
