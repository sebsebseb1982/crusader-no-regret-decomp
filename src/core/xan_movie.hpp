#pragma once
// No Regret's movies, FLICS/*.AVI: RIFF AVI files with Origin's "Xxan"
// codec (XANPLAY.C, segments 1440..1478, and the 32-bit decoder
// XANDRV32.DLL that REGRET.EXE loads and calls through the thunks of
// 1478:0541..055D). The port reads the whole file at once, where the
// original streams it (the reader of segments 1458..1470).
//
// Header (1470:02C9): "avih", then the video "strl" (its "strf" holds the
// width and height) with a "MOVE" list of:
//   "_PC_"  u32 version (3 or 4), u32 (1 or 2)   -> streamer +0x4A, +0x56
//   "PAL8"  256 RGB triples                       (vtable +0x0C)
//   "BEST"  the colour table, packed              (vtable +0x10, 1470:0E92)
// and the audio "strl" (format 0x594A, Xan DPCM: 22050 Hz, mono or stereo,
// 16-bit; WEAVED.AVI has none). "movi" holds "00db" frames and "01wb" audio
// chunks, one each per frame at 15 frames a second.
//
// The decoder (XANDRV32.DLL, table at its start: create 0x4AA, decode 0x256B,
// draw 0x25F2...) keeps a luma plane (one byte a pixel, 0..63) and a chroma
// plane (one word per 2 x 2 pixels). The 8-bit output (mode 3) looks a pixel
// up in the 64 KB colour table at (chroma word | luma): the chroma word is
// u << 11 | v << 6, two signed 5-bit components.
//
// A frame ("00db"):
//   u32 type (0: whole picture, 1: changes to the previous one)
//   u32 chroma offset (from +4; 0: the chroma is kept)
//   then, from +8: u32 offset of the luma corrections (from +8, 0: none),
//   for type 1 a second u32 offset of corrections to add to them, then the
//   luma (Huffman-coded, 0xDC1) at +12 (type 0) or +16 (type 1).
// The packed parts (chroma indices, corrections, BEST) use the LZ scheme of
// 0xEDE.

#include "core/surface.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace crusader {

class XanMovie {
public:
    static constexpr std::uint32_t kAudioRate = 22050;
    static constexpr int kFrameRate = 15;

    struct Chunk {
        std::uint32_t id = 0;
        std::size_t offset = 0;
        std::uint32_t size = 0;
    };

    // Throws std::runtime_error when the file cannot be read or is not an
    // "Xxan" AVI file.
    static XanMovie load(const std::filesystem::path& path);
    static XanMovie parse(std::vector<std::uint8_t> bytes);

    int width() const { return width_; }
    int height() const { return height_; }
    // "_PC_" (streamer +0x4A): 4 for most movies, 3 for some, whose palette
    // and chroma are stored differently (see XanDecoder and the player).
    int version() const { return version_; }
    // "PAL8": 768 bytes, 8-bit components from version 4, 6-bit (with stray
    // high bits) before.
    const std::array<std::uint8_t, 768>& palette() const { return palette_; }
    // "BEST" unpacked: the palette index of each (chroma | luma).
    const std::vector<std::uint8_t>& colour_table() const { return table_; }
    // 0 without sound, else 1 or 2.
    int audio_channels() const { return audio_channels_; }

    // The "00db" and "01wb" chunks of "movi", in order.
    const std::vector<Chunk>& chunks() const { return chunks_; }
    std::span<const std::uint8_t> data(const Chunk& c) const {
        return std::span<const std::uint8_t>(bytes_).subspan(c.offset, c.size);
    }

    static constexpr std::uint32_t kFrameChunk = 0x62643030;  // "00db"
    static constexpr std::uint32_t kAudioChunk = 0x62773130;  // "01wb"

private:
    std::vector<std::uint8_t> bytes_;
    int width_ = 0, height_ = 0;
    int version_ = 4;
    std::array<std::uint8_t, 768> palette_{};
    std::vector<std::uint8_t> table_;
    int audio_channels_ = 0;
    std::vector<Chunk> chunks_;
};

// XANDRV32 0xEDE: the LZ unpacker. Reads `src` from `pos` to its end code;
// throws std::out_of_range on a broken stream or more than `limit` bytes.
std::vector<std::uint8_t> xan_unpack(std::span<const std::uint8_t> src, std::size_t pos, std::size_t limit);

// XANDRV32 0xDC1: the Huffman decoder of the luma (u8 nodes, u8 end code,
// nodes x 2 bytes of tree, then the bits, most significant first).
std::vector<std::uint8_t> xan_huffman(std::span<const std::uint8_t> src, std::size_t pos, std::size_t limit);

class XanDecoder {
public:
    // XANDRV32 0x4AA (create, mode 3: 8-bit through `table`).
    XanDecoder(int width, int height, int version, std::vector<std::uint8_t> table);

    // XANDRV32 0x256B. `skip_corrections`: "lowresmovies=true" (DS:75A6),
    // or a frame decoded late, which is not drawn either.
    void decode(std::span<const std::uint8_t> frame, bool skip_corrections = false);

    // XANDRV32 0x25F2 in mode 3: 0xFFA, or 0x1A4F when `doubled` (each pixel
    // twice, one screen row in two: the rows between are left as they are).
    void draw(Surface8& screen, int x, int y, bool doubled) const;

    std::vector<std::uint8_t>& colour_table() { return table_; }
    const std::vector<std::uint8_t>& luma() const { return luma_; }
    const std::vector<std::uint16_t>& chroma() const { return chroma_; }

private:
    void decode_chroma(std::vector<std::uint8_t>& frame, std::size_t src);
    void apply_corrections(std::span<const std::uint8_t> corrections);

    int width_, height_, version_;
    std::vector<std::uint8_t> table_;
    std::vector<std::uint8_t> luma_;     // ctx +0x2C
    std::vector<std::uint16_t> chroma_;  // ctx +0x30, width / 2 words a row
};

// An audio chunk as signed 8-bit mono samples: 1478:0198 (mono) or 1478:02AC
// (stereo, the two channels averaged), the routines the player picks for an
// 8-bit mono output (1440:018E). Xan DPCM: one signed 16-bit start value per
// channel (the first sample), then one byte per sample and channel.
std::vector<std::int8_t> xan_audio_samples(std::span<const std::uint8_t> chunk, int channels);

}  // namespace crusader
