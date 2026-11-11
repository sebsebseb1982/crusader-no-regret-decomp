#include "core/xan_movie.hpp"

#include "core/bytes.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace crusader {

namespace {

constexpr std::uint32_t id(const char (&s)[5]) {
    return static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[0])) |
           static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[1])) << 8 |
           static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[2])) << 16 |
           static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[3])) << 24;
}

struct RiffChunk {
    std::uint32_t id;
    std::size_t data;
    std::uint32_t size;
};

std::vector<RiffChunk> riff_chunks(std::span<const std::uint8_t> d, std::size_t from, std::size_t to) {
    std::vector<RiffChunk> out;
    std::size_t p = from;
    to = std::min(to, d.size());
    while (p + 8 <= to) {
        const std::uint32_t cid = read_u32(d, p);
        const std::uint32_t size = read_u32(d, p + 4);
        if (size > to - p - 8) break;
        out.push_back({cid, p + 8, size});
        p += 8 + size + (size & 1);
    }
    return out;
}

bool is_list(std::span<const std::uint8_t> d, const RiffChunk& c, std::uint32_t type) {
    return c.id == id("LIST") && c.size >= 4 && read_u32(d, c.data) == type;
}

void copy_literals(std::vector<std::uint8_t>& dst, std::span<const std::uint8_t> src, std::size_t& pos,
                   std::size_t n) {
    check_range(src, pos, n);
    dst.insert(dst.end(), src.begin() + static_cast<std::ptrdiff_t>(pos), src.begin() + static_cast<std::ptrdiff_t>(pos + n));
    pos += n;
}

}  // namespace

// @orig none: whole file read at once (the original streams it, 1458/1470)
XanMovie XanMovie::load(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path.string());
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return parse(std::move(bytes));
}

// @orig 1470:02C9 1470:0E92
// The MOVE list's "_PC_", "SHOT" and "INDX" are read by the header parser
// (jump table 1470:0D89); "PAL8" and "BEST" are looked up when the player
// asks for them (vtable +0x0C, +0x10), always the first ones (index 0). A
// movie with a second pair (ANIM01, ABPMCMVA: "_PC_" 4, 2) has "SHOT" give
// the frame from which it applies; 1470:136A finds the pair of each frame
// read and 1468:04C8 keeps it (player +0x22), but nothing reads it: the
// first pair stays for the whole movie. A movie without "_PC_" keeps the
// streamer's version 0, like the version 3 movies.
XanMovie XanMovie::parse(std::vector<std::uint8_t> bytes) {
    XanMovie m;
    m.bytes_ = std::move(bytes);
    m.version_ = 0;
    const std::span<const std::uint8_t> d = m.bytes_;
    if (d.size() < 12 || read_u32(d, 0) != id("RIFF") || read_u32(d, 8) != id("AVI ")) {
        throw std::runtime_error("not an AVI file");
    }
    bool video = false, best = false, palette = false;
    for (const RiffChunk& top : riff_chunks(d, 12, d.size())) {
        if (is_list(d, top, id("hdrl"))) {
            for (const RiffChunk& c : riff_chunks(d, top.data + 4, top.data + top.size)) {
                if (!is_list(d, c, id("strl"))) continue;
                std::uint32_t type = 0;
                for (const RiffChunk& s : riff_chunks(d, c.data + 4, c.data + c.size)) {
                    if (s.id == id("strh")) {
                        type = read_u32(d, s.data);
                        if (type == id("vids") && read_u32(d, s.data + 4) != id("Xxan"))
                            throw std::runtime_error("AVI file with an unknown codec");
                    } else if (s.id == id("strf") && type == id("vids")) {
                        video = true;
                        m.width_ = read_i32(d, s.data + 4);
                        m.height_ = read_i32(d, s.data + 8);
                    } else if (s.id == id("strf") && type == id("auds")) {
                        // 1470:02C9: the format 0x594A is Xan DPCM.
                        if (read_u16(d, s.data) == 0x594A) m.audio_channels_ = read_u16(d, s.data + 2);
                    } else if (is_list(d, s, id("MOVE"))) {
                        for (const RiffChunk& v : riff_chunks(d, s.data + 4, s.data + s.size)) {
                            if (v.id == id("_PC_")) {
                                m.version_ = read_i32(d, v.data);
                            } else if (v.id == id("PAL8") && !palette) {
                                palette = true;
                                check_range(d, v.data, std::min<std::size_t>(v.size, 768));
                                std::copy_n(d.begin() + static_cast<std::ptrdiff_t>(v.data),
                                            std::min<std::size_t>(v.size, 768), m.palette_.begin());
                            } else if (v.id == id("BEST") && !best) {
                                best = true;
                                m.table_ = xan_unpack(d.subspan(0, v.data + v.size), v.data, 0x10000);
                                m.table_.resize(0x10000);
                            }
                        }
                    }
                }
            }
        } else if (is_list(d, top, id("movi"))) {
            for (const RiffChunk& c : riff_chunks(d, top.data + 4, top.data + top.size)) {
                if (c.id == kFrameChunk || c.id == kAudioChunk) m.chunks_.push_back({c.id, c.data, c.size});
            }
        }
    }
    if (!video || m.width_ <= 0 || m.height_ <= 0 || m.width_ > 640 || m.height_ > 480)
        throw std::runtime_error("AVI file without an Xxan video stream");
    // XANDRV32 0xD5A: mode 3 without a colour table fails (error 2).
    if (!best) throw std::runtime_error("Xxan movie without a colour table");
    return m;
}

// @orig XANDRV32.DLL 0xEDE
std::vector<std::uint8_t> xan_unpack(std::span<const std::uint8_t> src, std::size_t pos, std::size_t limit) {
    std::vector<std::uint8_t> dst;
    for (;;) {
        const std::uint8_t op = read_u8(src, pos++);
        std::size_t offset = 0, length = 0;
        if (!(op & 0x80)) {
            const std::uint8_t b = read_u8(src, pos++);
            copy_literals(dst, src, pos, op & 3);
            offset = ((op & 0x60u) << 3) + b + 1;
            length = ((op & 0x1Cu) >> 2) + 3;
        } else if (!(op & 0x40)) {
            const std::uint8_t b0 = read_u8(src, pos), b1 = read_u8(src, pos + 1);
            pos += 2;
            copy_literals(dst, src, pos, b0 >> 6);
            offset = ((b0 & 0x3Fu) << 8) + b1 + 1;
            length = (op & 0x3Fu) + 4;
        } else if (!(op & 0x20)) {
            const std::uint8_t b0 = read_u8(src, pos), b1 = read_u8(src, pos + 1), b2 = read_u8(src, pos + 2);
            pos += 3;
            copy_literals(dst, src, pos, op & 3);
            offset = (((op & 0x10u) >> 4) << 16) + (std::size_t{b0} << 8) + b1 + 1;
            length = b2 + 5 + ((op & 0x0Cu) << 6);
        } else {
            const std::size_t n = ((op & 0x1Fu) << 2) + 4;
            if (n <= 0x70) {
                copy_literals(dst, src, pos, n);
                if (dst.size() > limit) throw std::out_of_range("Xan data unpacks too long");
                continue;
            }
            copy_literals(dst, src, pos, op & 3);
            if (dst.size() > limit) throw std::out_of_range("Xan data unpacks too long");
            return dst;
        }
        if (offset > dst.size()) throw std::out_of_range("Xan back reference before the start");
        for (std::size_t k = 0; k < length; ++k) dst.push_back(dst[dst.size() - offset]);
        if (dst.size() > limit) throw std::out_of_range("Xan data unpacks too long");
    }
}

// @orig XANDRV32.DLL 0xDC1
// The node is kept in a 32-bit register of which only the low byte is
// replaced: a node 128 or above would leave a high bit there. The movies'
// trees never get that far (their roots stay well below 128).
std::vector<std::uint8_t> xan_huffman(std::span<const std::uint8_t> src, std::size_t pos, std::size_t limit) {
    const std::uint32_t nodes = read_u8(src, pos), end = read_u8(src, pos + 1);
    const std::size_t tree = pos + 2;  // node n's children at tree + 2 * (n - end - 1)
    std::size_t bits = tree + 2 * nodes;
    const std::uint32_t root = (nodes + end) & 0xFF;
    std::vector<std::uint8_t> out;
    std::uint32_t node = nodes + end;
    for (;;) {
        const std::uint8_t byte = read_u8(src, bits++);
        for (int k = 7; k >= 0; --k) {
            node = node * 2 + ((byte >> k) & 1);
            const std::size_t at = tree + node - 2 * (end + 1);
            if (node < 2 * (end + 1)) throw std::out_of_range("Xan Huffman node below the tree");
            node = (node & ~0xFFu) | read_u8(src, at);
            if ((node & 0xFF) == end) return out;
            if ((node & 0xFF) < end) {
                out.push_back(static_cast<std::uint8_t>(node));
                if (out.size() > limit) throw std::out_of_range("Xan Huffman data too long");
                node = (node & ~0xFFu) | root;
            }
        }
    }
}

// @orig XANDRV32.DLL 0x4AA
XanDecoder::XanDecoder(int width, int height, int version, std::vector<std::uint8_t> table)
    : width_(width),
      height_(height),
      version_(version),
      table_(std::move(table)),
      luma_(static_cast<std::size_t>(width) * height),
      chroma_(static_cast<std::size_t>(width / 2) * ((height + 1) / 2)) {
    table_.resize(0x10000);
}

// @orig XANDRV32.DLL 0x30 0x24DB
void XanDecoder::decode_chroma(std::vector<std::uint8_t>& f, std::size_t src) {
    if (version_ >= 4) {
        // 0x24DB: the table's two signed bytes to a chroma word; positive
        // components lose 2 first (0 at least).
        const int count = read_u8(f, src + 2);
        for (int k = 0; k < count; ++k) {
            const std::size_t q = src + 4 + 2 * static_cast<std::size_t>(k);
            check_range(f, q, 2);
            std::uint8_t b[2] = {f[q], f[q + 1]};
            for (std::uint8_t& c : b) {
                if (c < 0x80) {
                    c = static_cast<std::uint8_t>(c - 2);
                    if (c > 0x80) c = 0;
                }
            }
            const auto word = static_cast<std::uint16_t>((b[1] & 0xF8) << 8 | (b[0] & 0xF8) << 3);
            f[q] = static_cast<std::uint8_t>(word);
            f[q + 1] = static_cast<std::uint8_t>(word >> 8);
        }
    }
    const std::uint16_t mode = read_u16(f, src);
    const std::size_t count = read_u16(f, src + 2);
    const std::size_t table = src + 2;  // entry k at table + 2 k
    const std::vector<std::uint8_t> packed = xan_unpack(f, src + 4 + 2 * count, chroma_.size());
    auto entry = [&](std::uint8_t k) { return read_u16(f, table + 2 * std::size_t{k}); };
    if (mode != 0) {
        // Every chroma word, in pairs from the end (an odd first byte is left).
        for (std::ptrdiff_t e = static_cast<std::ptrdiff_t>(packed.size()) - 2; e >= 0; e -= 2) {
            const auto i = static_cast<std::size_t>(e);
            chroma_.at(i) = entry(packed[i]);
            chroma_.at(i + 1) = entry(packed[i + 1]);
        }
        return;
    }
    // One index per 2 x 2 chroma words (4 x 4 pixels); 0 keeps them.
    const std::size_t row = static_cast<std::size_t>(width_ / 2);
    std::size_t p = 0;
    for (int r = 0; r < height_ / 4; ++r) {
        for (int c = 0; c < width_ / 4; ++c) {
            const std::uint8_t v = p < packed.size() ? packed[p] : 0;
            ++p;
            if (v == 0) continue;
            const std::uint16_t e = entry(v);
            const std::size_t at = 2 * static_cast<std::size_t>(r) * row + 2 * static_cast<std::size_t>(c);
            chroma_.at(at) = chroma_.at(at + 1) = e;
            chroma_.at(at + row) = chroma_.at(at + row + 1) = e;
        }
    }
    // A height not a multiple of 4: the last row copied down.
    const int extra = (((height_ + 1) & ~1) - (height_ & ~3)) / 2;
    const std::size_t last = static_cast<std::size_t>(height_ / 4) * 2;
    for (int k = 0; k < extra && last > 0; ++k) {
        std::copy_n(chroma_.begin() + static_cast<std::ptrdiff_t>((last - 1) * row), row,
                    chroma_.begin() + static_cast<std::ptrdiff_t>((last + k) * row));
    }
}

// The corrections of the interpolated pixels (the odd ones): each byte,
// doubled (the four bytes of a dword at once, carries included), is added to
// a pixel modulo 64.
// @orig XANDRV32.DLL 0x1B1
void XanDecoder::apply_corrections(std::span<const std::uint8_t> c) {
    const std::size_t n = c.size() & ~std::size_t{1};
    for (std::size_t k = 0; k < n; k += 4) {
        std::uint32_t word = 0;
        for (std::size_t j = 0; j < 4 && k + j < c.size(); ++j) word |= std::uint32_t{c[k + j]} << (8 * j);
        word <<= 1;
        for (std::size_t j = 0; j < 4 && k + j < n; ++j) {
            const std::size_t p = 1 + 2 * (k + j);
            if (p >= luma_.size()) return;
            luma_[p] = static_cast<std::uint8_t>((luma_[p] + (word >> (8 * j))) & 0x3F);
        }
    }
}

// @orig XANDRV32.DLL 0x256B 0xF6 0x249
void XanDecoder::decode(std::span<const std::uint8_t> frame, bool skip_corrections) {
    std::vector<std::uint8_t> f(frame.begin(), frame.end());  // 0x24DB rewrites the chroma table
    const std::uint32_t type = read_u32(f, 0);
    const std::uint32_t chroma = read_u32(f, 4);
    if (chroma != 0) decode_chroma(f, std::size_t{chroma} + 4);
    const std::size_t base = 8;
    const int w = width_, h = height_;
    const std::size_t limit = static_cast<std::size_t>(w) * h + 4;
    std::uint8_t* l = luma_.data();
    if (type == 0) {
        const std::vector<std::uint8_t> s = xan_huffman(f, base + 4, limit);
        std::size_t p = 0;
        auto sym = [&] { return p < s.size() ? s[p++] : (++p, std::uint8_t{0}); };
        std::size_t o = 0;
        // The first row: even pixels from deltas, odd ones their sums.
        std::uint8_t a = sym();
        l[o++] = static_cast<std::uint8_t>(a * 2);
        for (int k = 0; k < (w - 2) / 2; ++k) {
            const std::uint8_t prev = a;
            a = static_cast<std::uint8_t>((a + sym()) & 0x1F);
            l[o++] = static_cast<std::uint8_t>(prev + a);
            l[o++] = static_cast<std::uint8_t>(a * 2);
        }
        l[o++] = static_cast<std::uint8_t>(a * 2);
        // The other rows: deltas from the even pixel above.
        for (int y = 1; y < h; ++y) {
            a = static_cast<std::uint8_t>(((l[o - w] >> 1) + sym()) & 0x1F);
            l[o++] = static_cast<std::uint8_t>(a * 2);
            for (int k = 0; k < (w - 2) / 2; ++k) {
                const std::uint8_t prev = a;
                a = static_cast<std::uint8_t>(((l[o + 1 - w] >> 1) + sym()) & 0x1F);
                l[o++] = static_cast<std::uint8_t>(prev + a);
                l[o++] = static_cast<std::uint8_t>(a * 2);
            }
            l[o++] = static_cast<std::uint8_t>(a * 2);
        }
        const std::uint32_t corrections = read_u32(f, base);
        if (!skip_corrections && corrections != 0) apply_corrections(xan_unpack(f, base + corrections, limit));
        return;
    }
    // Type 1: deltas (doubled) to the previous frame's even pixels, the odd
    // ones their averages.
    const std::vector<std::uint8_t> s = xan_huffman(f, base + 8, limit);
    std::size_t p = 0;
    auto sym = [&] { return p < s.size() ? s[p++] : (++p, std::uint8_t{0}); };
    std::size_t o = 0;
    for (int y = 0; y < h; ++y) {
        std::uint8_t al = static_cast<std::uint8_t>((static_cast<std::uint8_t>(sym() * 2) + l[o]) & 0x3F);
        l[o++] = al;
        for (int k = 0; k < (w - 2) / 2; ++k) {
            const auto ah = static_cast<std::uint8_t>((static_cast<std::uint8_t>(sym() * 2) + l[o + 1]) & 0x3F);
            l[o + 1] = ah;
            l[o] = static_cast<std::uint8_t>((al + ah) >> 1);
            o += 2;
            al = ah;
        }
        l[o++] = al;
    }
    const std::uint32_t corrections = read_u32(f, base);
    if (skip_corrections || corrections == 0) return;
    std::vector<std::uint8_t> c = xan_unpack(f, base + corrections, limit);
    if (const std::uint32_t more = read_u32(f, base + 4); more != 0) {
        // A second set added to the first, dword by dword (carries included).
        const std::vector<std::uint8_t> c2 = xan_unpack(f, base + more, limit);
        for (std::size_t k = 0; k < c.size(); k += 4) {
            std::uint32_t x = 0, y = 0;
            for (std::size_t j = 0; j < 4; ++j) {
                if (k + j < c.size()) x |= std::uint32_t{c[k + j]} << (8 * j);
                if (k + j < c2.size()) y |= std::uint32_t{c2[k + j]} << (8 * j);
            }
            x += y;
            for (std::size_t j = 0; j < 4 && k + j < c.size(); ++j) c[k + j] = static_cast<std::uint8_t>(x >> (8 * j));
        }
    }
    apply_corrections(c);
}

// @orig XANDRV32.DLL 0x25F2 0xFFA 0x1A4F
void XanDecoder::draw(Surface8& screen, int x0, int y0, bool doubled) const {
    const std::size_t row = static_cast<std::size_t>(width_ / 2);
    const int scale = doubled ? 2 : 1;
    for (int y = 0; y < height_; ++y) {
        const int sy = y0 + y * scale;
        if (sy < 0 || sy >= screen.height) continue;
        std::uint8_t* dst = screen.row(sy);
        const std::uint8_t* l = luma_.data() + static_cast<std::size_t>(y) * width_;
        const std::uint16_t* c = chroma_.data() + static_cast<std::size_t>(y / 2) * row;
        for (int x = 0; x < width_; ++x) {
            const std::uint8_t colour = table_[(c[x / 2] | l[x]) & 0xFFFF];
            for (int k = 0; k < scale; ++k) {
                const int sx = x0 + x * scale + k;
                if (sx >= 0 && sx < screen.width) dst[sx] = colour;
            }
        }
    }
}

namespace {

// The shift of one channel (the byte's low two bits): 3 adds one, 1 and 2
// take two or four off, down to 0.
// @orig 1478:0198
void xan_step(std::uint8_t b, std::uint8_t& shift) {
    const int code = b & 3;
    if (code == 0) return;
    shift = static_cast<std::uint8_t>(shift - 2 * code);
    if (code == 3) shift = static_cast<std::uint8_t>(shift + 7);
    else if (static_cast<std::int8_t>(shift) < 0) shift = 0;
}

std::int32_t xan_delta(std::uint8_t b, std::uint8_t shift) {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>((b & 0xFC) << 8)) >> (shift & 0x1F);
}

}  // namespace

// @orig 1478:0198 1478:02AC
std::vector<std::int8_t> xan_audio_samples(std::span<const std::uint8_t> chunk, int channels) {
    std::vector<std::int8_t> out;
    if (channels == 1) {
        if (chunk.size() < 2) return out;
        std::uint16_t v = read_u16(chunk, 0);  // 16-bit, wrapping
        out.push_back(static_cast<std::int8_t>(v >> 8));
        std::uint8_t shift = 4;
        for (std::size_t p = 2; p < chunk.size(); ++p) {
            xan_step(chunk[p], shift);
            v = static_cast<std::uint16_t>(v + xan_delta(chunk[p], shift));
            out.push_back(static_cast<std::int8_t>(v >> 8));
        }
        return out;
    }
    if (channels != 2 || chunk.size() < 4) return out;
    // 32-bit sums here: the average of the two channels, its high byte.
    std::int32_t left = static_cast<std::int16_t>(read_u16(chunk, 0));
    std::int32_t right = static_cast<std::int16_t>(read_u16(chunk, 2));
    auto sample = [&] {
        return static_cast<std::int8_t>(static_cast<std::uint8_t>(static_cast<std::uint32_t>((left + right) >> 1) >> 8));
    };
    out.push_back(sample());
    std::uint8_t shift_left = 4, shift_right = 4;
    for (std::size_t p = 4; p + 1 < chunk.size(); p += 2) {
        xan_step(chunk[p], shift_left);
        left += xan_delta(chunk[p], shift_left);
        xan_step(chunk[p + 1], shift_right);
        right += xan_delta(chunk[p + 1], shift_right);
        out.push_back(sample());
    }
    return out;
}

}  // namespace crusader
