#pragma once
// The Asylum Sound System (ASYLUM.DLL, "Written by Jason Ely", 1994-95), as
// the game drives it through _ASS_PlaySFX, _ASS_StopSFX, _ASS_PlayMusic...
// and mixes it from ASSProcess (_ASS_UpdateDAC) into the Sound Blaster DMA
// buffer.
//
// The mixer works one buffer at a time, one music tick long (DS:00A1 frames:
// mix rate / 50, then set by the module's tempo). For each buffer:
//   1. unless the music is stopped or paused, the music player runs one tick
//      and its 8 channels are mixed at the music volume (DS:0096);
//   2. the sound effect requests are applied (No Regret 1000:1921): _ASS_PlaySFX
//      only posts one, so a sound starts at the next buffer, as do a stop and
//      a volume change;
//   3. the 12 sound effect channels are mixed at the sound volume (DS:00CC);
//      one that reaches its end is free again (1018:07F6, which also calls
//      the game back: sound_mark_stopped).
// A channel's volume v (0..64) is scaled by the music or sound volume g
// (0..64): v * g / 64 (table at DS:C926); 0x41 bypasses it at full scale.
// The sound effect step is (rate << 16) / mix rate + pitch (pitch at most
// 0x358; the game passes 0).
//
// The output (Output below) is picked by the card: No Remorse's DLL has one
// mono routine per sample size and the port takes the 16-bit one
// (No Remorse 1000:0D6C). No Regret's DLL also mixes in stereo, which its
// Sound Blaster 16 driver always asks for (1038:006D, DS:00D0 = 1), in 16
// bits when the card's DMA channel is a 16-bit one ("dma" of CRUSADER.CFG,
// 4 and up, 1018:058F), in 8 bits otherwise:
//   - 16 bits (1000:1EC0): each channel through its own routine (+44): its
//     two shares, or the surround of E80 (asylum_voice.hpp);
//   - 8 bits (1000:2144): the music channels through fixed routines, 0 and
//     7 in surround, 1 and 6 on both outputs, 2 and 4 on the second only,
//     3 and 5 on the first only (E8x is lost); the sound effects as in 16
//     bits. Each output sum + 128 goes through a clip table (DS:40A4) to an
//     unsigned byte.
// A sound effect starts at its volume as both shares (No Regret 1000:1921:
// for its first buffer the level is v * g / 64 * v / 64), then at full shares.
// The sums are then copied out; with oversampling ("filter=true" in
// CRUSADER.CFG, the default) each output is the mean of the sum and the
// previous one, per output: (sum + 0x8000) / 2 + the previous half in 16
// bits, byte / 2 + the previous half in 8 bits.
//
// Not reproduced: 1018:07F6 starts at once, in the same buffer, a sound the
// game's callback posts again on the channel that ended (the audio stream of
// 1450:0000); the game's callback posts none.
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

    enum class Output {
        kMono16,    // No Remorse 1000:0D6C
        kStereo16,  // No Regret 1000:1EC0
        kStereo8,   // No Regret 1000:2144
    };

    AsylumMixer();

    // The card's output; mix() then writes channels() samples a frame.
    void set_output(Output output);
    Output output() const;
    int channels() const;

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

    // Fills `out` with `frames` frames of 16-bit samples at kMixRate,
    // channels() samples each (first output first).
    void mix(std::int16_t* out, std::size_t frames);
    // Frames left in the current buffer (0: the next mix starts a buffer).
    std::size_t buffered() const;

    // For tests: the player state (not locked).
    const AsylumMusic& music() const { return music_; }

private:
    // A posted sound effect (0x17 bytes at DS:0624 in No Regret).
    struct Request {
        int state = 0;  // 0 free, 1 posted, 2 playing
        SoundSample sample;
        bool loop = false;
        int volume = 0;
        std::uint32_t step = 0;
    };

    static int level(int volume, int global);
    int channels_locked() const { return output_ == Output::kMono16 ? 1 : 2; }
    SampleDepth depth() const { return output_ == Output::kStereo8 ? SampleDepth::k8 : SampleDepth::k16; }
    bool mix_voice(AsylumVoice& v, std::int32_t* sum, std::size_t frames, int global) const;
    std::int16_t output_sample(std::int32_t sum, int channel);
    void fill();
    void apply_requests();

    mutable std::mutex mutex_;
    Output output_ = Output::kMono16;
    std::array<Request, kChannels> requests_{};
    std::array<AsylumVoice, kChannels> voices_{};
    AsylumMusic music_{kMixRate};
    bool music_paused_ = false;  // DS:0099
    int sound_volume_ = kMaxVolume;
    int music_volume_ = 0x20;
    bool filter_ = true;
    // The filter's last half of each output: (sum + 0x8000) / 2 in 16 bits,
    // byte / 2 in 8 bits.
    std::array<int, 2> previous_half_{0x4000, 0x4000};
    std::vector<std::int32_t> buffer_;  // frames of channels() sums
    std::size_t next_ = 0;  // read position in buffer_
};

}  // namespace crusader
