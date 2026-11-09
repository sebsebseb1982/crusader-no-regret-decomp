#pragma once
// SNDMGR.C (segment 12F0): the game's 12 voices over the Asylum mixer's
// channels. Voices 0-7 play one-shot sounds, 8-11 looping ones. A voice is
// { handle, sfx, owner, volume, flags }; each play gets a new handle from a
// counter (DS:46F9), so a stale handle no longer finds its voice.
//
// sound_alloc_voice (12F0:0367): the first free voice of the range; when
// none is, the voice at the range's round-robin index (DS:46F5 for one-shot,
// DS:46F7 for looping) is taken over, its sound source released and its
// channel stopped, and the index moves on.
//
// A channel that reaches the end of its sample calls sound_mark_stopped
// (12F0:04AE, registered with _ASS_RegisterCallback): the voice is flagged,
// and sound_update (12F0:0428) frees it at the next call. The port asks the
// mixer instead of being called back.
//
// Not reproduced: the sound cache (SNDCACHE.C) that loads the samples into
// a fixed pool on demand and evicts the least recently used one, stopping
// its voices; SOUND.FLX stays in memory here.

#include "core/sound_flex.hpp"
#include "engine/audio/asylum_mixer.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

namespace crusader {

class SoundManager {
public:
    using Handle = std::int32_t;
    static constexpr Handle kNoHandle = -1;
    static constexpr int kVoices = AsylumMixer::kChannels;
    static constexpr int kOneShotVoices = 8;

    SoundManager(const SoundFlex& flex, AsylumMixer& mixer);

    // 12F0:0136: plays `sfx` on a voice, kNoHandle when it cannot. `owner`
    // is 0xFF for the interface sounds, 0 for the sound sources.
    Handle play(int sfx, int volume, int owner, bool loop);
    // 12F0:026A: plays `sfx` again on the voice of `handle` (one-shot), or
    // on a new voice when the handle is stale.
    Handle retrigger(Handle handle, int sfx, int volume, int owner);
    void stop(Handle handle);   // 12F0:02EB
    void stop_all();            // 12F0:032E
    void stop_sfx(int sfx);     // 12F0:04D8: every voice playing it
    void set_volume(Handle handle, int volume);  // 12F0:0543
    bool is_playing(Handle handle) const;        // 12F0:022B
    // 12F0:057A: the sound the handle's voice plays, -1 when it is over.
    int sfx_of(Handle handle) const;
    // 12F0:05BC: the handle of a voice playing `sfx`, kNoHandle if none.
    Handle handle_playing(int sfx) const;
    // 12F0:0428: frees the voices whose sound is over.
    void update();
    // The sample of `sfx` (SNDCACHE_12f8_052d), nullopt when there is none.
    std::optional<SoundSample> sample(int sfx) const { return flex_.sample(sfx); }

    // Called with the old handle when a voice is taken over
    // (sound_source_release_handle).
    void set_on_steal(std::function<void(Handle)> fn) { on_steal_ = std::move(fn); }

private:
    struct Voice {
        Handle handle = kNoHandle;
        int sfx = 0;
        int owner = 0;
        int volume = 0;
        bool loop = false;
        bool stopped = false;  // flag 2: over, freed by update()
    };

    int find(Handle handle) const;
    int allocate(bool loop);
    bool over(int voice) const;

    const SoundFlex& flex_;
    AsylumMixer& mixer_;
    std::array<Voice, kVoices> voices_{};
    int next_one_shot_ = 0;              // DS:46F5
    int next_loop_ = kOneShotVoices;     // DS:46F7
    Handle next_handle_ = 0;             // DS:46F9
    std::function<void(Handle)> on_steal_;
};

}  // namespace crusader
