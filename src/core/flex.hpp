#pragma once
// Origin "Flex" archive (.FLX), as read by FILE\FLEX.C in the original.
//
// Layout (little-endian):
//   0x00..0x51  comment padded with 0x1A
//   0x54        u32 entry count
//   0x80        index: count * { u32 offset, u32 size }, offset 0 = empty entry

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace crusader {

class FlexArchive {
public:
    static FlexArchive open(const std::filesystem::path& path);

    std::size_t count() const { return index_.size(); }
    std::span<const std::uint8_t> entry(std::size_t i) const;
    const std::string& comment() const { return comment_; }

private:
    struct Slot {
        std::uint32_t offset;
        std::uint32_t size;
    };
    std::vector<std::uint8_t> data_;
    std::vector<Slot> index_;
    std::string comment_;
};

// Builds a Flex archive the way the saved games are laid out: entries in the
// order they are added after the index, a comment of 0x1A bytes, then the
// entry count and a 1 (at 0x58).
class FlexWriter {
public:
    explicit FlexWriter(std::size_t count) : index_(count) {}
    void add(std::size_t i, std::span<const std::uint8_t> bytes);
    std::vector<std::uint8_t> bytes() const;
    // Written to `path`.tmp then renamed over `path`.
    void write(const std::filesystem::path& path) const;

private:
    struct Slot {
        std::uint32_t offset = 0;
        std::uint32_t size = 0;
    };
    std::vector<Slot> index_;
    std::vector<std::uint8_t> data_;
};

std::vector<std::uint8_t> read_file(const std::filesystem::path& path);


}  // namespace crusader
