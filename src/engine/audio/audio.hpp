#pragma once
// DSFXMAN.C (segment 12E0): the sound system as the game sees it, set up by
// dsfx_init when CRUSADER.CFG says "sound=on" (DS:89E0): SOUND.FLX, the
// voices (SNDMGR) and 32 sound sources (SNDSRC). Every caller in the
// original first checks that it is set up (DS:462A); here the world holds a
// null pointer without sound.

#include "core/sound_flex.hpp"
#include "engine/audio/asylum_mixer.hpp"
#include "engine/audio/music.hpp"
#include "engine/audio/sound_manager.hpp"
#include "engine/audio/sound_sources.hpp"

#include <filesystem>

namespace crusader {

class Audio {
public:
    // `sound_dir` holds the music (SOUND/*.AMF).
    explicit Audio(SoundFlex flex, std::filesystem::path sound_dir = {})
        : flex_(std::move(flex)), music(mixer, std::move(sound_dir)) {}
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    // AudioProcess::I_playSFX (12E0:0476, intrinsics 0x0AB and 0x0D4): an
    // interface sound, full volume, one-shot.
    void play_sfx(int sfx) { voices.play(sfx, AsylumMixer::kMaxVolume, 0xFF, false); }
    // 12E0:050D: stops the voice playing `sfx`, if any.
    void stop_sfx(int sfx) {
        const SoundManager::Handle h = voices.handle_playing(sfx);
        if (h != SoundManager::kNoHandle) voices.stop(h);
    }
    // 12E0:0539 / AudioProcess::I_stopAllSFX (0x0D1).
    void stop_all() { voices.stop_all(); }

    const SoundFlex& flex() const { return flex_; }

private:
    SoundFlex flex_;

public:
    AsylumMixer mixer;
    SoundManager voices{flex_, mixer};
    SoundSources sources{voices};
    Music music;
};

}  // namespace crusader
