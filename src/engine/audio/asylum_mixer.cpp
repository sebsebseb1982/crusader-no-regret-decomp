#include "engine/audio/asylum_mixer.hpp"

#include <algorithm>

namespace crusader {

namespace {

bool valid(int channel) { return channel >= 0 && channel < AsylumMixer::kChannels; }

// 1000:2144: the music channels' shares (first, second) in 8-bit stereo,
// written before mixing channels 1..6; 0 and 7 go through the surround.
constexpr std::array<std::array<int, 2>, AsylumMusic::kChannels> kStereo8Shares = {{
    {0, 0}, {0x40, 0x40}, {0, 0x40}, {0x40, 0}, {0, 0x40}, {0x40, 0}, {0x40, 0x40}, {0, 0},
}};

}  // namespace

AsylumMixer::AsylumMixer() = default;

// @orig ASYLUM.DLL:1018:058F ASYLUM.DLL:1038:0050
void AsylumMixer::set_output(Output output) {
    std::lock_guard lock(mutex_);
    if (output == output_) return;
    output_ = output;
    buffer_.clear();
    next_ = 0;
    // The filter's halves before the first output: DS:0CB3 (16 bits) and
    // DS:0CB1 (8 bits) of No Regret's DLL.
    switch (output) {
    case Output::kMono16:
        previous_half_ = {0x4000, 0x4000};
        break;
    case Output::kStereo16:
        previous_half_ = {0, 0};
        break;
    case Output::kStereo8:
        previous_half_ = {0x50, 0};
        break;
    }
}

AsylumMixer::Output AsylumMixer::output() const {
    std::lock_guard lock(mutex_);
    return output_;
}

int AsylumMixer::channels() const {
    std::lock_guard lock(mutex_);
    return channels_locked();
}

// The table at DS:C926: v * g / 64; 0x41 stays.
int AsylumMixer::level(int volume, int global) {
    if (volume == AsylumVoice::kFullScale) return AsylumVoice::kFullScale;
    return std::clamp(volume, 0, kMaxVolume) * global / kMaxVolume;
}

// @orig ASYLUM.DLL:1018:0693 ASYLUM.DLL:1010:0054
void AsylumMixer::play(int channel, const SoundSample& sample, int volume, int pitch, bool loop) {
    if (!valid(channel)) return;
    std::lock_guard lock(mutex_);
    Request& r = requests_[static_cast<std::size_t>(channel)];
    r.sample = sample;
    r.loop = loop;
    r.volume = volume;
    r.step = (static_cast<std::uint32_t>(sample.rate) << 16) / kMixRate + static_cast<std::uint32_t>(std::min(pitch, 0x358));
    r.state = 1;
}

void AsylumMixer::stop(int channel) {
    if (!valid(channel)) return;
    std::lock_guard lock(mutex_);
    requests_[static_cast<std::size_t>(channel)].state = 0;
}

void AsylumMixer::stop_all() {
    std::lock_guard lock(mutex_);
    for (Request& r : requests_) r.state = 0;
}

bool AsylumMixer::busy(int channel) const {
    if (!valid(channel)) return false;
    std::lock_guard lock(mutex_);
    return requests_[static_cast<std::size_t>(channel)].state != 0;
}

void AsylumMixer::set_volume(int channel, int volume) {
    if (!valid(channel)) return;
    std::lock_guard lock(mutex_);
    requests_[static_cast<std::size_t>(channel)].volume = volume;
}

void AsylumMixer::set_sound_volume(int volume) {
    std::lock_guard lock(mutex_);
    sound_volume_ = std::clamp(volume, 0, kMaxVolume);
}

int AsylumMixer::sound_volume() const {
    std::lock_guard lock(mutex_);
    return sound_volume_;
}

void AsylumMixer::set_filter(bool on) {
    std::lock_guard lock(mutex_);
    filter_ = on;
}

void AsylumMixer::play_music(std::shared_ptr<const AmfModule> module) {
    std::lock_guard lock(mutex_);
    music_.load(std::move(module));
    music_.start();
}

void AsylumMixer::stop_music() {
    std::lock_guard lock(mutex_);
    music_.stop();
}

void AsylumMixer::pause_music(bool paused) {
    std::lock_guard lock(mutex_);
    music_paused_ = paused;
}

void AsylumMixer::set_music_volume(int volume) {
    std::lock_guard lock(mutex_);
    music_volume_ = std::clamp(volume, 0, kMaxVolume);
}

int AsylumMixer::music_volume() const {
    std::lock_guard lock(mutex_);
    return music_volume_;
}

bool AsylumMixer::music_playing() const {
    std::lock_guard lock(mutex_);
    return music_.playing();
}

std::size_t AsylumMixer::buffered() const {
    std::lock_guard lock(mutex_);
    return (buffer_.size() - next_) / static_cast<std::size_t>(channels_locked());
}

// @orig ASYLUM.DLL:1000:1921
void AsylumMixer::apply_requests() {
    for (std::size_t i = 0; i < requests_.size(); ++i) {
        Request& r = requests_[i];
        AsylumVoice& v = voices_[i];
        switch (r.state) {
        case 0:
            v.active = false;
            break;
        case 1:
            r.state = 2;
            v = AsylumVoice{};
            v.pcm = r.sample.pcm;
            v.length = static_cast<std::uint32_t>(r.sample.pcm.size());
            if (r.loop) {
                v.loop_start = r.sample.loop_start;
                v.loop_length = r.sample.loop_length;
            }
            v.step = r.step;
            v.volume = r.volume;
            // The shares take the volume for this first buffer.
            v.first_share = v.second_share = r.volume;
            v.active = true;
            break;
        default:
            v.volume = r.volume;
            v.first_share = v.second_share = AsylumVoice::kFullShare;
            if (!v.active) r.state = 0;
            break;
        }
    }
}

// One channel through its routine (+44 in No Regret) at the global volume g.
// @orig ASYLUM.DLL:1000:1EC0
bool AsylumMixer::mix_voice(AsylumVoice& v, std::int32_t* sum, std::size_t frames, int global) const {
    const int at = level(v.volume, global);
    if (output_ == Output::kMono16) return v.mix(sum, frames, at);
    if (v.surround) return v.mix_surround(sum, frames, at, depth());
    if (at == AsylumVoice::kFullScale) return v.mix_stereo(sum, frames, at, at, depth());
    return v.mix_stereo(sum, frames, level(at, v.first_share), level(at, v.second_share), depth());
}

// No Remorse 1000:0D6C for the mono output (No Regret's own mono routines,
// 1000:1A0C and 1C59, are for the cards the port does not offer).
// @orig ASYLUM.DLL:1000:1EC0 ASYLUM.DLL:1000:2144
void AsylumMixer::fill() {
    const std::size_t frames = static_cast<std::size_t>(std::max(music_.tick_length(), 1));
    buffer_.assign(frames * static_cast<std::size_t>(channels_locked()), 0);
    next_ = 0;
    std::int32_t* sum = buffer_.data();
    if (music_.playing() && !music_paused_) {
        music_.tick();
        auto& music = music_.voices();
        for (std::size_t i = 0; i < music.size(); ++i) {
            AsylumVoice& v = music[i];
            if (output_ != Output::kStereo8) {
                mix_voice(v, sum, frames, music_volume_);
            } else if (i == 0 || i == music.size() - 1) {
                v.mix_surround(sum, frames, level(v.volume, music_volume_), SampleDepth::k8);
            } else {
                v.first_share = kStereo8Shares[i][0];
                v.second_share = kStereo8Shares[i][1];
                const int at = level(v.volume, music_volume_);
                v.mix_stereo(sum, frames, level(at, v.first_share), level(at, v.second_share), SampleDepth::k8);
            }
        }
    }
    apply_requests();
    for (std::size_t i = 0; i < voices_.size(); ++i) {
        if (!mix_voice(voices_[i], sum, frames, sound_volume_)) requests_[i].state = 0;
    }
}

// The copy at the end of each buffer routine.
// @orig ASYLUM.DLL:1000:1EC0 ASYLUM.DLL:1000:2144
std::int16_t AsylumMixer::output_sample(std::int32_t sum, int channel) {
    int& previous = previous_half_[static_cast<std::size_t>(channel)];
    if (output_ == Output::kStereo8) {
        // The clip table DS:40A4 (1018:0532) holds sums -0x480..0x47F: the
        // port clips them all.
        int byte = std::clamp(sum + 0x80, 0, 0xFF);
        if (filter_) {
            const int half = byte >> 1;
            byte = previous + half;
            previous = half;
        }
        return static_cast<std::int16_t>((byte - 0x80) * 0x100);
    }
    // The original sums into 16 bits and wraps; clamped here.
    const int s = std::clamp(sum, -32768, 32767);
    if (!filter_) return static_cast<std::int16_t>(s);
    const int half = (s + 0x8000) >> 1;
    const int out = previous + half - 0x8000;
    previous = half;
    return static_cast<std::int16_t>(out);
}

// @orig ASYLUM.DLL:1018:0103 ASYLUM.DLL:1000:1EC0 ASYLUM.DLL:1000:2144
void AsylumMixer::mix(std::int16_t* out, std::size_t frames) {
    std::lock_guard lock(mutex_);
    const int n = channels_locked();
    for (std::size_t i = 0; i < frames; ++i) {
        if (next_ >= buffer_.size()) fill();
        for (int c = 0; c < n; ++c) *out++ = output_sample(buffer_[next_++], c);
    }
}

}  // namespace crusader
