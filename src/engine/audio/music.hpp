#pragma once
// The game's side of the music (STDINT.C, segment 1200; "MusicProcess" in
// the usecode): which module plays, from SOUND/<name>.AMF.
//
// Tracks (table of names at DS:3C03): 1..15 "m01".."m15", 16 "m16a",
// 17 "cred" (the credits), 18 "menu" (the menu). Track 16 moves on to the
// next of m16a, m16b, m16c each time it is asked for, before loading: the
// first play is m16b. 0 and the others are silence.
//
// Playing a track stops the current one, loads the module (amf_load) and
// starts it, the music paused meanwhile (DS:3BFB bit 3) so the mixer does
// not run a half-loaded module. The current track (DS:8624) and the flags
// (DS:3BFB, bit 0: playing) are saved in entries 70 and 71 of a saved game;
// loading plays the saved track again from its start.
//
// The menu (IRGUMP_13D0_0000 / 03F5) and the credits (Credits_init /
// Credits_done) play their own track over the game's: push() keeps the
// current one on a stack of 4 (DS:861C), pop() plays it again from its
// start.

#include "engine/audio/asylum_mixer.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string>

namespace crusader {

class Music {
public:
    static constexpr int kCredits = 0x11;
    static constexpr int kMenu = 0x12;

    // No Regret: I_playMusic(0x45) plays the track of the player's map.
    static constexpr int kMapMusic = 0x45;

    Music(AsylumMixer& mixer, std::filesystem::path sound_dir);

    // The tracks of No Regret (the default) or of No Remorse. No Regret
    // (STDINT 1210:0100) names 21 tracks in a table (DS:45E1: "ninth", "phil"...
    // 17 is "cred" and 18 "regret", the menu's), does nothing when asked for
    // the track that plays, and has no "m16a/b/c" rotation.
    void set_remorse(bool remorse) { remorse_ = remorse; }
    // No Regret, 1210:0186: the track of a map (DS:463D), 1 from map 26 on.
    static int map_track(int map);
    // No Regret, 1210:0000: the date is 24 to 31 December (or the cheat that
    // forces it): the map music is then "xmas". Set by the program from the
    // clock; never by the tests.
    void set_christmas(bool christmas) { christmas_ = christmas; }
    // DS:46C9: Christmas forced, whatever the date (around the joke code's
    // music, cheats.hpp); DS:46CA: the Christmas music allowed (1 at start,
    // 0 once Ctrl+M stepped the tracks).
    void force_christmas(bool forced) { christmas_forced_ = forced; }
    void allow_christmas(bool allowed) { christmas_allowed_ = allowed; }
    // 1210:0000.
    bool christmas() const { return christmas_forced_ || christmas_; }
    // I_playMusic of No Regret (1210:01A2): `track`, or the player's map's
    // when it is kMapMusic.
    void play_regret(int track, int player_map);

    // MusicProcess::I_playMusic (1200:0143, intrinsic 0x012).
    void play(int track);
    // MusicProcess::I_stopMusic (1200:0223, intrinsic 0x091).
    void stop();
    // 1200:0262: plays `track`, keeping the current one (if any) to come
    // back to.
    void push(int track);
    // 1200:028E: plays the last kept track again.
    void pop();
    // 1200:02AD: forgets the kept tracks, silence.
    void reset();

    // Loading a saved game (1200:02BF, No Regret 1210:0381): entries 70 and 71.
    void restore(int track, int flags);

    int current() const { return current_; }
    int flags() const { return flags_; }
    // The file name of a track ("m01"...), nullopt for silence. Track 16
    // gives the current one of m16a / m16b / m16c.
    std::optional<std::string> file_name(int track) const;

    // 1200:01E7 / 0205 (music_pause / music_resume): _ASS_PauseMusic and
    // _ASS_ResumeMusic, flag 8 of DS:3BFB (also used by the video player).
    void pause();
    void resume();

private:
    void start(int track, const std::optional<std::string>& name);

    AsylumMixer& mixer_;
    std::filesystem::path dir_;
    char m16_ = 'a';  // the letter of "m16a" (DS:3C43 + 3)
    bool remorse_ = false;
    bool christmas_ = false;
    bool christmas_forced_ = false;
    bool christmas_allowed_ = true;
    int current_ = 0;  // DS:91A4 (DS:8624 in No Remorse)
    int flags_ = 0;    // DS:3BFB
    std::array<int, 4> stack_{};  // DS:861C
    int depth_ = 0;               // DS:3BFD
};

}  // namespace crusader
