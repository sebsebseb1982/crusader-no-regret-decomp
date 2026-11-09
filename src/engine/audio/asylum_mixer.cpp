#include "engine/audio/asylum_mixer.hpp"

#include <algorithm>

namespace crusader {

namespace {

constexpr int kBypassVolume = 0x41;

bool valid(int channel) { return channel >= 0 && channel < AsylumMixer::kChannels; }

}  // namespace

AsylumMixer::AsylumMixer() = default;

int AsylumMixer::scale(int volume, int global) {
    if (volume == kBypassVolume) return 2 * kMaxVolume;
    return std::clamp(volume, 0, kMaxVolume) * global / kMaxVolume;
}

// @orig ASYLUM.DLL:1018:0629
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
    return buffer_.size() - next_;
}

// @orig ASYLUM.DLL:1000:0A42
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
            v.active = true;
            break;
        default:
            v.volume = r.volume;
            if (!v.active) r.state = 0;
            break;
        }
    }
}

// @orig ASYLUM.DLL:1000:0D6C
void AsylumMixer::fill() {
    buffer_.assign(static_cast<std::size_t>(std::max(music_.tick_length(), 1)), 0);
    next_ = 0;
    if (music_.playing() && !music_paused_) {
        music_.tick();
        for (AsylumVoice& v : music_.voices()) v.mix(buffer_.data(), buffer_.size(), scale(v.volume, music_volume_));
    }
    apply_requests();
    for (std::size_t i = 0; i < voices_.size(); ++i) {
        AsylumVoice& v = voices_[i];
        if (!v.mix(buffer_.data(), buffer_.size(), scale(v.volume, sound_volume_))) requests_[i].state = 0;
    }
}

// @orig ASYLUM.DLL:1018:00F6 ASYLUM.DLL:1000:0D6C
void AsylumMixer::mix(std::int16_t* out, std::size_t count) {
    std::lock_guard lock(mutex_);
    for (std::size_t i = 0; i < count; ++i) {
        if (next_ >= buffer_.size()) fill();
        // The original sums into 16 bits and wraps; clamped here.
        const int s = std::clamp(buffer_[next_++], -32768, 32767);
        if (filter_) {
            const int half = (s + 0x8000) >> 1;
            out[i] = static_cast<std::int16_t>(previous_half_ + half - 0x8000);
            previous_half_ = half;
        } else {
            out[i] = static_cast<std::int16_t>(s);
        }
    }
}

}  // namespace crusader
