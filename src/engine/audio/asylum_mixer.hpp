#pragma once
// The Asylum Sound System (ASYLUM.DLL, "Written by Jason Ely", 1994-95), as
// the game drives it through _ASS_PlaySFX, _ASS_StopSFX, _ASS_PlayMusic...
// and mixes it from ASSProcess (_ASS_UpdateDAC) into the Sound Blaster DMA
// buffer.
//
// The mixer works one buffer at a time, one music tick long (DS:00A1 samples:
// mix rate / 50, then set by the module's tempo). For each buffer (1000:0D6C,
// 16-bit cards):
//   1. unless the music is stopped or paused, the music player runs one tick
//      and its 8 channels are mixed at the music volume (DS:0096);
//   2. the sound effect requests are applied (1000:0A42): _ASS_PlaySFX only
//      posts one, so a sound starts at the next buffer, as do a stop and a
//      volume change;
//   3. the 12 sound effect channels are mixed at the sound volume (DS:00CC);
//      one that reaches its end is free again (1018:078C, which also calls
//      the game back: sound_mark_stopped).
// A channel's volume v (0..64) is scaled by the music or sound volume g
// (0..64): v * g / 64 (table at DS:CD00); 0x41 bypasses it at double scale.
// The sound effect step is (rate << 16) / mix rate + pitch (pitch at most
// 0x358; the game passes 0).
// The sum is then copied out; with oversampling ("filter=true" in
// CRUSADER.CFG, the default) each output is the mean of the sum and the
// previous one.
//
// Thread-safe: the game posts requests, the audio device pulls mix().

#include "core/amf_module.hpp"
#include "core/sound_flex.hpp"
#include "engine/audio/asylum_music.hpp"
#include "engine/audio/asylum_voice.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace crusader {

class AsylumMixer {
public:
    static constexpr int kChannels = 12;
    static constexpr int kMixRate = 22050;  // DS:4648, "mixrate" in CRUSADER.CFG
    static constexpr int kMaxVolume = 0x40;

    AsylumMixer();

    // _ASS_PlaySFX: replaces whatever the channel plays, from the next
    // buffer. `loop` uses the sample's loop points.
    void play(int channel, const SoundSample& sample, int volume, int pitch, bool loop);
    void stop(int channel);  // _ASS_StopSFX
    void stop_all();         // _ASS_StopAllSFX
    bool busy(int channel) const;  // _ASS_IsChannelBusy: requested or playing
    void set_volume(int channel, int volume);  // _ASS_SetSFXVolume
    void set_sound_volume(int volume);  // _ASS_SetSoundVolume (DS:464B, 0x40)
    int sound_volume() const;
    void set_filter(bool on);  // _ASS_SetOversampling / _ASS_ClrOversampling

    // _ASS_PlayMusic then _ASS_StartMusic: plays `module` from the start.
    void play_music(std::shared_ptr<const AmfModule> module);
    void stop_music();  // _ASS_StopMusic
    void pause_music(bool paused);  // _ASS_PauseMusic / _ASS_ResumeMusic
    void set_music_volume(int volume);  // _ASS_SetMusicVolume (DS:464D, 0x20)
    int music_volume() const;
    bool music_playing() const;

    // Fills `out` with mono 16-bit samples at kMixRate.
    void mix(std::int16_t* out, std::size_t count);
    // Samples left in the current buffer (0: the next mix starts a buffer).
    std::size_t buffered() const;

    // For tests: the player state (not locked).
    const AsylumMusic& music() const { return music_; }

private:
    // A posted sound effect (0x17 bytes at DS:0814).
    struct Request {
        int state = 0;  // 0 free, 1 posted, 2 playing
        SoundSample sample;
        bool loop = false;
        int volume = 0;
        std::uint32_t step = 0;
    };

    static int scale(int volume, int global);
    void fill();
    void apply_requests();

    mutable std::mutex mutex_;
    std::array<Request, kChannels> requests_{};
    std::array<AsylumVoice, kChannels> voices_{};
    AsylumMusic music_{kMixRate};
    bool music_paused_ = false;  // DS:0099
    int sound_volume_ = kMaxVolume;
    int music_volume_ = 0x20;
    bool filter_ = true;
    int previous_half_ = 0x4000;  // last (sum + 0x8000) / 2 of the filter
    std::vector<std::int32_t> buffer_;
    std::size_t next_ = 0;  // read position in buffer_
};

}  // namespace crusader
