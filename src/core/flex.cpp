#include "core/flex.hpp"

#include "core/bytes.hpp"

#include <fstream>
#include <stdexcept>

namespace crusader {

// @orig none: file reading helper
std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open " + path.string());
    }
    in.seekg(0, std::ios::end);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(in.tellg()));
    in.seekg(0);
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return data;
}

// @orig 1248:0142 1248:01C8
FlexArchive FlexArchive::open(const std::filesystem::path& path) {
    constexpr std::size_t kCountOffset = 0x54;
    constexpr std::size_t kIndexOffset = 0x80;

    FlexArchive flex;
    flex.data_ = read_file(path);
    const auto& d = flex.data_;
    if (d.size() < kIndexOffset) {
        throw std::runtime_error(path.string() + ": too small for a Flex archive");
    }

    for (std::size_t i = 0; i < 0x52 && d[i] != 0; ++i) {
        if (d[i] != 0x1A) {
            flex.comment_ += static_cast<char>(d[i]);
        }
    }

    const std::uint32_t count = read_u32(d, kCountOffset);
    if (kIndexOffset + std::size_t{count} * 8 > d.size()) {
        throw std::runtime_error(path.string() + ": index exceeds file size");
    }
    flex.index_.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        Slot s{read_u32(d, kIndexOffset + 8 * i), read_u32(d, kIndexOffset + 8 * i + 4)};
        if (s.offset != 0 && std::size_t{s.offset} + s.size > d.size()) {
            throw std::runtime_error(path.string() + ": entry " + std::to_string(i) + " out of bounds");
        }
        flex.index_.push_back(s);
    }
    return flex;
}

// @orig ?: Flex writing (FASTFLEX_1228_0000), layout taken from the saved games
void FlexWriter::add(std::size_t i, std::span<const std::uint8_t> bytes) {
    if (i >= index_.size()) throw std::runtime_error("Flex entry " + std::to_string(i) + " out of range");
    const std::size_t start = 0x80 + index_.size() * 8;
    index_[i] = {static_cast<std::uint32_t>(start + data_.size()), static_cast<std::uint32_t>(bytes.size())};
    data_.insert(data_.end(), bytes.begin(), bytes.end());
}

// @orig ?: Flex writing (FASTFLEX_1228_0000), layout taken from the saved games
std::vector<std::uint8_t> FlexWriter::bytes() const {
    std::vector<std::uint8_t> out(0x80 + index_.size() * 8, 0);
    std::fill_n(out.begin(), 0x52, std::uint8_t{0x1A});
    auto u32 = [&](std::size_t at, std::uint32_t v) {
        for (int b = 0; b < 4; ++b) out[at + b] = static_cast<std::uint8_t>(v >> (8 * b));
    };
    u32(0x54, static_cast<std::uint32_t>(index_.size()));
    u32(0x58, 1);
    for (std::size_t i = 0; i < index_.size(); ++i) {
        u32(0x80 + 8 * i, index_[i].offset);
        u32(0x80 + 8 * i + 4, index_[i].size);
    }
    out.insert(out.end(), data_.begin(), data_.end());
    return out;
}

// @orig none: file writing of the port
void FlexWriter::write(const std::filesystem::path& path) const {
    const auto data = bytes();
    std::filesystem::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("cannot write " + tmp.string());
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!out) throw std::runtime_error("cannot write " + tmp.string());
    }
    std::filesystem::rename(tmp, path);
}

// @orig 1248:02A8

std::span<const std::uint8_t> FlexArchive::entry(std::size_t i) const {
    if (i >= index_.size() || index_[i].offset == 0) {
        return {};
    }
    return std::span(data_).subspan(index_[i].offset, index_[i].size);
}

}  // namespace crusader
