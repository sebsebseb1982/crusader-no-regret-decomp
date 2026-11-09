#pragma once
// SNDSRC.C (segment 1450): sounds attached to items, as the scripts and the
// engine play them (AudioProcess::I_playSFXCru and its relatives). 32
// sources (dsfx_init); the last 4 are for looping sounds, the others for
// one-shot ones. A source is { handle, item, sfx, volume, initial volume,
// looping }, 11 bytes in the original.
//
// The volume comes from the item's place (1450:0000): 0x40 when it is partly
// on screen, else by the larger of |dx|, |dy| to the player (DS:7E0E):
// 0x32 up to 0x17C, 0x20 up to 0x280, 0x10 up to 0x3C0, 8 beyond. Only the
// looping sources follow the player afterwards (1450:0813, at each camera
// move); a one-shot sound keeps its first volume.
//
// Sound numbers outside 1..0x200 are ignored.

#include "engine/audio/sound_manager.hpp"
#include "engine/world/world.hpp"

#include <array>
#include <cstdint>

namespace crusader {

class SoundSources {
public:
    static constexpr int kSources = 32;
    static constexpr int kLoopSources = 4;

    explicit SoundSources(SoundManager& voices);

    // 1450:021E (I_playSFXCru, 0x015): a one-shot sound at the item, when a
    // source is free.
    void play(const World& world, int sfx, ObjId item);
    // 1450:02C5 (I_playAmbientSFXCru, 0x02A): a sound without a loop plays as
    // above; a looping one only if no source plays it yet, on a loop source.
    void play_ambient(const World& world, int sfx, ObjId item);
    // 1450:03AD (0x045): restarts the item's sound on its voice, or plays it.
    void retrigger(const World& world, int sfx, ObjId item);
    // 1450:0486 (I_stopSFXCru, 0x03B): stops the first source of the item
    // playing `sfx`.
    void stop(int sfx, ObjId item);
    // 1450:0551 (0x077): stops every source of the item.
    void stop_item(ObjId item);
    // 1450:05B1 (Item::I_destroy): stops the item's looping sources.
    void stop_item_loops(ObjId item);
    // 1450:050B (Item::I_destroy, NPCs): the item's sounds play on, detached.
    void detach(ObjId item);
    // 1450:0633 (I_isSFXPlayingForObject, 0x03D).
    bool is_playing(int sfx, ObjId item);
    // 1450:06C0: an attached source plays `sfx`.
    bool is_playing_anywhere(int sfx);
    // 1450:0750 (10A8:3A13): sets the volume of the item's source of `sfx`.
    void set_volume(int sfx, ObjId item, int volume);
    // 1450:0813 (after each camera move): the looping sources' volumes
    // follow the player.
    void update_volumes(const World& world);
    void clear();

    // 1450:0000.
    static int volume_for(const World& world, ObjId item);

private:
    struct Source {
        SoundManager::Handle handle = SoundManager::kNoHandle;
        ObjId item = 0;
        int sfx = 0;
        int volume = 0;
        int initial_volume = 0;
        bool loop = false;
    };

    // 1450:01D0: frees the sources whose voice no longer plays their sound.
    void refresh();
    void release(SoundManager::Handle handle);  // 1450:07B9
    void start(Source& s, const World& world, int sfx, ObjId item, bool loop);
    static bool valid_sfx(int sfx) { return sfx > 0 && sfx <= 0x200; }

    SoundManager& voices_;
    std::array<Source, kSources> sources_{};
};

}  // namespace crusader
