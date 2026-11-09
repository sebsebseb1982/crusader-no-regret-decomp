#include "engine/audio/sound_manager.hpp"

namespace crusader {

SoundManager::SoundManager(const SoundFlex& flex, AsylumMixer& mixer) : flex_(flex), mixer_(mixer) {}

// @orig 12F0:0102
int SoundManager::find(Handle handle) const {
    if (handle == kNoHandle) return -1;
    for (int v = 0; v < kVoices; ++v) {
        if (voices_[static_cast<std::size_t>(v)].handle == handle) return v;
    }
    return -1;
}

bool SoundManager::over(int voice) const {
    return voices_[static_cast<std::size_t>(voice)].stopped || !mixer_.busy(voice);
}

// @orig 12F0:0367
int SoundManager::allocate(bool loop) {
    const int first = loop ? kOneShotVoices : 0;
    const int end = loop ? kVoices : kOneShotVoices;
    int voice = -1;
    for (int v = first; v < end; ++v) {
        if (voices_[static_cast<std::size_t>(v)].handle == kNoHandle) {
            voice = v;
            break;
        }
    }
    if (voice == -1) {
        int& next = loop ? next_loop_ : next_one_shot_;
        voice = next;
        if (++next == end) next = first;
        if (on_steal_) on_steal_(voices_[static_cast<std::size_t>(voice)].handle);
        mixer_.stop(voice);
    }
    voices_[static_cast<std::size_t>(voice)].handle = next_handle_++;
    return voice;
}

// @orig 12F0:0136
SoundManager::Handle SoundManager::play(int sfx, int volume, int owner, bool loop) {
    update();
    const int v = allocate(loop);
    Voice& voice = voices_[static_cast<std::size_t>(v)];
    voice.sfx = sfx;
    voice.owner = owner;
    voice.volume = volume;
    voice.loop = loop;
    voice.stopped = false;
    if (const auto sample = flex_.sample(sfx)) mixer_.play(v, *sample, volume, 0, loop);
    else mixer_.stop(v);
    return voice.handle;
}

// @orig 12F0:026A
SoundManager::Handle SoundManager::retrigger(Handle handle, int sfx, int volume, int owner) {
    if (handle == kNoHandle) return kNoHandle;
    const int v = find(handle);
    if (v == -1) return play(sfx, volume, owner, false);
    // The voice keeps its sound number and volume.
    if (const auto sample = flex_.sample(sfx)) {
        mixer_.play(v, *sample, voices_[static_cast<std::size_t>(v)].volume, 0, false);
    }
    return handle;
}

// @orig 12F0:02EB
void SoundManager::stop(Handle handle) {
    const int v = find(handle);
    if (v == -1) return;
    Voice& voice = voices_[static_cast<std::size_t>(v)];
    voice.stopped = true;
    mixer_.stop(v);
    voice.handle = kNoHandle;
}

void SoundManager::stop_all() {
    for (const Voice& voice : voices_) {
        if (voice.handle != kNoHandle) stop(voice.handle);
    }
}

// @orig 12F0:04D8
void SoundManager::stop_sfx(int sfx) {
    for (int v = 0; v < kVoices; ++v) {
        const Voice& voice = voices_[static_cast<std::size_t>(v)];
        if (voice.sfx != sfx) continue;
        mixer_.stop(v);
        if (voice.handle != kNoHandle) {
            if (on_steal_) on_steal_(voice.handle);
            stop(voice.handle);
        }
    }
}

// @orig 12F0:0543
void SoundManager::set_volume(Handle handle, int volume) {
    const int v = find(handle);
    if (v == -1) return;
    voices_[static_cast<std::size_t>(v)].volume = volume;
    mixer_.set_volume(v, volume);
}

bool SoundManager::is_playing(Handle handle) const {
    const int v = find(handle);
    return v != -1 && !over(v);
}

int SoundManager::sfx_of(Handle handle) const {
    const int v = find(handle);
    if (v == -1 || over(v)) return -1;
    return voices_[static_cast<std::size_t>(v)].sfx;
}

// @orig 12F0:05BC
SoundManager::Handle SoundManager::handle_playing(int sfx) const {
    for (const Voice& voice : voices_) {
        if (voice.handle != kNoHandle && voice.sfx == sfx) return voice.handle;
    }
    return kNoHandle;
}

// @orig 12F0:0428
void SoundManager::update() {
    for (int v = 0; v < kVoices; ++v) {
        Voice& voice = voices_[static_cast<std::size_t>(v)];
        if (voice.handle != kNoHandle && over(v)) voice = Voice{};
    }
}

}  // namespace crusader
