#include "core/avi_movie.hpp"

#include "core/bytes.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace crusader {

namespace {

// DS:69C2 and DS:69B2: length of run code c, and the extra bits after it.
// Codes 3, 7, 11 and 15 do not occur in the movies.
constexpr std::array<std::uint32_t, 16> kRunBase = {0, 128, 8, 0, 2, 0, 32, 0, 1, 256, 16, 0, 4, 0, 64, 0};
constexpr std::array<int, 16> kRunExtraBits = {0, 7, 3, 0, 1, 16, 5, 0, 1, 8, 4, 0, 2, 24, 6, 0};

// FUN_1460_0000: the next `n` bits, most significant first.
class BitReader {
public:
    explicit BitReader(std::span<const std::uint8_t> d) : d_(d) {}

    // @orig 1460:0000
    std::uint32_t get(int n) {
        std::uint32_t window = 0;
        const std::size_t byte = pos_ >> 3;
        for (std::size_t i = 0; i < 4; ++i) {
            window = window << 8 | (byte + i < d_.size() ? d_[byte + i] : 0);
        }
        window <<= pos_ & 7;
        pos_ += static_cast<std::size_t>(n);
        return n == 0 ? 0 : window >> (32 - n);
    }

private:
    std::span<const std::uint8_t> d_;
    std::size_t pos_ = 0;
};

struct RiffChunk {
    std::uint32_t id;
    std::size_t data;
    std::uint32_t size;
};

// @orig 1468:2188
std::vector<RiffChunk> riff_chunks(std::span<const std::uint8_t> d, std::size_t from, std::size_t to) {
    std::vector<RiffChunk> out;
    std::size_t p = from;
    while (p + 8 <= to) {
        const std::uint32_t id = read_u32(d, p);
        const std::uint32_t size = read_u32(d, p + 4);
        if (size > to - p - 8) break;
        out.push_back({id, p + 8, size});
        p += 8 + size + (size & 1);
    }
    return out;
}

bool is_list(std::span<const std::uint8_t> d, const RiffChunk& c, std::uint32_t type) {
    return c.id == fourcc("LIST") && c.size >= 4 && read_u32(d, c.data) == type;
}

}  // namespace

// @orig none: whole file read at once
AviMovie AviMovie::load(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path.string());
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return parse(std::move(bytes));
}

// @orig 1468:2188 1468:17B0
AviMovie AviMovie::parse(std::vector<std::uint8_t> bytes) {
    AviMovie m;
    m.bytes_ = std::move(bytes);
    const std::span<const std::uint8_t> d = m.bytes_;
    if (d.size() < 12 || read_u32(d, 0) != fourcc("RIFF") || read_u32(d, 8) != fourcc("AVI ")) {
        throw std::runtime_error("not an AVI file");
    }
    bool video = false;
    for (const RiffChunk& top : riff_chunks(d, 12, d.size())) {
        if (is_list(d, top, fourcc("hdrl"))) {
            for (const RiffChunk& c : riff_chunks(d, top.data + 4, top.data + top.size)) {
                if (c.id == fourcc("avih")) {
                    m.width_ = static_cast<int>(read_u32(d, c.data + 32));
                    m.height_ = static_cast<int>(read_u32(d, c.data + 36));
                }
                if (!is_list(d, c, fourcc("strl"))) continue;
                std::uint32_t type = 0;
                for (const RiffChunk& s : riff_chunks(d, c.data + 4, c.data + c.size)) {
                    if (s.id == fourcc("strh")) {
                        type = read_u32(d, s.data);
                        if (type == fourcc("vids")) {
                            m.codec_ = read_u32(d, s.data + 4);
                            m.frame_count_ = read_u32(d, s.data + 32);
                        } else if (type == fourcc("auds")) {
                            m.has_audio_ = true;
                            m.audio_initial_frames_ = read_u32(d, s.data + 16);
                        }
                    } else if (s.id == fourcc("strf") && type == fourcc("vids")) {
                        video = true;
                        m.colours_ = static_cast<int>(read_u32(d, s.data + 36));
                        for (int i = 0; i < 256 && 40u + 4u * i + 4u <= s.size; ++i) {
                            const std::size_t q = s.data + 40 + 4 * static_cast<std::size_t>(i);
                            m.colour_table_[i] = {d[q + 2], d[q + 1], d[q]};
                        }
                    }
                }
            }
        } else if (is_list(d, top, fourcc("movi"))) {
            for (const RiffChunk& c : riff_chunks(d, top.data + 4, top.data + top.size)) {
                m.chunks_.push_back({c.id, c.data, c.size});
            }
        }
    }
    if (!video || m.width_ <= 0 || m.height_ <= 0) throw std::runtime_error("AVI file without a video stream");
    return m;
}

// @orig ?: strip decoder of VideoPlayer (segment 1468), not split yet
void decode_movie_strip(std::span<const std::uint8_t> chunk, int strip, int strips, bool half_width,
                        Surface8& frame) {
    const std::size_t start = read_u32(chunk, 4 * static_cast<std::size_t>(strip));
    const std::uint32_t codes_size = read_u32(chunk, start);
    if (codes_size == 0) return;
    BitReader codes(chunk.subspan(start + 4));
    std::size_t src = start + 4 + codes_size;

    const int rows = frame.height / strips;
    std::size_t out = static_cast<std::size_t>(strip) * rows * frame.width;
    const std::size_t end = out + static_cast<std::size_t>(rows) * frame.width;
    bool copy = true;  // switched before the first run, which skips
    std::uint32_t run = 0;
    while (out < end) {
        if (run == 0) {
            const std::uint32_t code = codes.get(4);
            run = kRunBase[code];
            if (code != 0 && code != 8) run += codes.get(kRunExtraBits[code]);
            if (half_width) run <<= 1;
            copy = !copy;
            continue;
        }
        const std::size_t n = std::min<std::size_t>(run, end - out);
        if (copy) {
            if (half_width) {
                check_range(chunk, src, (n + 1) >> 1);
                for (std::size_t k = 0; k < n; ++k) frame.pixels[out + k] = chunk[src + (k >> 1)];
                src += n >> 1;
            } else {
                check_range(chunk, src, n);
                std::copy_n(chunk.begin() + static_cast<std::ptrdiff_t>(src), n, frame.pixels.begin() + static_cast<std::ptrdiff_t>(out));
                src += n;
            }
        }
        out += n;
        run -= static_cast<std::uint32_t>(n);
    }
}

std::vector<std::int8_t> movie_audio_samples(std::span<const std::uint8_t> chunk) {
    std::vector<std::int8_t> out(chunk.size());
    for (std::size_t i = 0; i < chunk.size(); ++i) out[i] = static_cast<std::int8_t>(chunk[i] - 0x80);
    return out;
}

}  // namespace crusader
