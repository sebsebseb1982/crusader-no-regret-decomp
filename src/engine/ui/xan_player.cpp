#include "engine/ui/xan_player.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace crusader {

namespace {

constexpr std::uint32_t kFramePeriodUs = 1'000'000 / XanMovie::kFrameRate;

}  // namespace

// @orig 1448:0B5A
std::string regret_movie_base_name(std::string_view name) {
    std::string up(name);
    for (char& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return up.size() < 3 ? "0" + up : up;
}

// @orig 1448:06F4
XanPlayer::XanPlayer(XanMovie movie, std::vector<RegretSubtitle> subtitles, bool big, bool low_res, bool fade,
                     const FontSet& fonts)
    : movie_(std::move(movie)),
      decoder_(movie_.width(), movie_.height(), movie_.version(), movie_.colour_table()),
      subtitles_(std::move(subtitles)),
      doubled_(big),
      low_res_(low_res),
      fade_(fade),
      fonts_(&fonts),
      screen_(kScreenWidth, kScreenHeight) {
    const int scale = doubled_ ? 2 : 1;
    x_ = (kScreenWidth - movie_.width() * scale) / 2;
    y_ = (kScreenHeight - movie_.height() * scale) / 2;
    set_palette();
    screen_.clear(darkest_);
    for (const XanMovie::Chunk& c : movie_.chunks()) {
        if (c.id != XanMovie::kAudioChunk) continue;
        const std::vector<std::int8_t> samples = xan_audio_samples(movie_.data(c), movie_.audio_channels());
        sound_.insert(sound_.end(), samples.begin(), samples.end());
    }
}

// @orig 1448:03B4
void XanPlayer::set_palette() {
    std::array<std::uint8_t, 768> vga = movie_.palette();
    for (std::uint8_t& c : vga) c = movie_.version() > 3 ? static_cast<std::uint8_t>(c >> 2) : c & 0x3F;
    auto component = [&](int i, int k) { return static_cast<int>(vga[3 * static_cast<std::size_t>(i) + k]); };
    // The colour nearest to colour 255, and the darkest one, among 0..254.
    int nearest = 0, nearest_distance = 0x7FFF, darkest_level = 0x7FFF;
    for (int i = 0; i < 255; ++i) {
        int distance = 0, level = 0;
        for (int k = 0; k < 3; ++k) {
            const int d = component(i, k) - component(255, k);
            distance += d * d;
            level += component(i, k) * component(i, k);
        }
        if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest = i;
        }
        if (level < darkest_level) {
            darkest_level = level;
            darkest_ = static_cast<std::uint8_t>(i);
        }
    }
    std::vector<std::uint8_t>& table = decoder_.colour_table();
    std::replace(table.begin(), table.end(), std::uint8_t{0xFF}, static_cast<std::uint8_t>(nearest));
    vga[3 * 255] = vga[3 * 255 + 1] = vga[3 * 255 + 2] = 63;
    palette_ = Palette::from_vga(vga);
}

bool XanPlayer::take_sound_start() { return std::exchange(sound_due_, false); }

// @orig 1468:04C8
void XanPlayer::advance(std::uint32_t elapsed_us, bool key_pressed) {
    if (finished_) return;
    if (!started_) {
        // The first frame at once, the sound with it.
        started_ = true;
        sound_due_ = !sound_.empty();
        next_frame(false);
        return;
    }
    elapsed_us_ += elapsed_us;
    // Frames due: the late ones decoded without corrections, not drawn.
    while (!finished_ && elapsed_us_ >= kFramePeriodUs) {
        elapsed_us_ -= kFramePeriodUs;
        next_frame(elapsed_us_ >= kFramePeriodUs);
    }
    // 1448:02BC: a key in the buffer stops the movie.
    if (key_pressed) finished_ = true;
}

// @orig 1448:01BA
void XanPlayer::next_frame(bool late) {
    while (chunk_ < movie_.chunks().size() && movie_.chunks()[chunk_].id != XanMovie::kFrameChunk) ++chunk_;
    if (chunk_ >= movie_.chunks().size()) {
        finished_ = true;
        return;
    }
    const XanMovie::Chunk& c = movie_.chunks()[chunk_++];
    try {
        decoder_.decode(movie_.data(c), late || low_res_);
    } catch (const std::out_of_range&) {
        // A broken frame: the original would draw whatever its buffers hold.
    }
    if (!late) decoder_.draw(screen_, x_, y_, doubled_);
    // 1440:0732: entries before this frame are passed.
    while (subtitle_ < subtitles_.size() && subtitles_[subtitle_].frame < frame_) ++subtitle_;
    if (subtitle_ < subtitles_.size() && subtitles_[subtitle_].frame == frame_) show_subtitle(subtitles_[subtitle_].text);
    ++frame_;
}

// @orig 1440:036C 1440:0570
void XanPlayer::show_subtitle(const std::string& text) {
    Surface8 strip(kScreenWidth, kSubtitleHeight, kSubtitleBackground);
    TextWriter writer(*fonts_, kSubtitleFont);
    const int width = writer.measure(text).width;
    writer.print(strip, (kScreenWidth - width) / 2, kSubtitleTextY, text);
    for (std::uint8_t& p : strip.pixels) p = p == 0 ? kWhite : darkest_;
    for (int y = 0; y < kSubtitleHeight; ++y) std::copy_n(strip.row(y), kScreenWidth, screen_.row(kSubtitleY + y));
}

}  // namespace crusader
