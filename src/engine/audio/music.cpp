#include "engine/audio/music.hpp"

#include "core/amf_module.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace crusader {

namespace {

// The files are "M01.AMF" on the CD: look the name up without the case, for
// case-sensitive file systems (Linux).
// @orig none: file lookup without the case, port addition
std::filesystem::path find_module(const std::filesystem::path& dir, const std::string& name) {
    const auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    };
    const std::string wanted = lower(name + ".amf");
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (lower(entry.path().filename().string()) == wanted) return entry.path();
    }
    return dir / (name + ".amf");
}

constexpr int kPlaying = 1;
constexpr int kPaused = 8;
constexpr int kM16 = 0x10;

}  // namespace

Music::Music(AsylumMixer& mixer, std::filesystem::path sound_dir) : mixer_(mixer), dir_(std::move(sound_dir)) {}

namespace {
// DS:45E1: the file names of No Regret's tracks 1..21.
constexpr const char* kRegretTracks[] = {nullptr,  "ninth",  "phil",   "straight", "party",  "demo",
                                         "stint",  "mk",     "space2", "d3",       "space",  "rhythm",
                                         "intent", "m03",    "silver", "m01",      "techno", "cred",
                                         "regret", "m13",    "retro",  "metal"};
// DS:463D: the track of maps 0..25.
constexpr int kMapTracks[] = {0, 1, 10, 2, 0, 3, 11, 4, 16, 5, 20, 6, 0, 7, 13, 8, 15, 9, 12, 10, 19, 14, 21, 0, 0, 2};
}  // namespace

// @orig 1210:0186
int Music::map_track(int map) {
    return map >= 0 && map < static_cast<int>(std::size(kMapTracks)) ? kMapTracks[map] : 1;
}

// @orig 1210:0100
std::optional<std::string> Music::file_name(int track) const {
    if (!remorse_) {
        if (track >= 1 && track < static_cast<int>(std::size(kRegretTracks))) return kRegretTracks[track];
        return std::nullopt;
    }
    switch (track) {
    case kM16:
        return std::string("m16") + m16_;
    case kCredits:
        return "cred";
    case kMenu:
        return "menu";
    default:
        if (track >= 1 && track <= 15) return (track < 10 ? "m0" : "m") + std::to_string(track);
        return std::nullopt;
    }
}

void Music::pause() {
    if (flags_ & kPaused) return;
    flags_ |= kPaused;
    mixer_.pause_music(true);
}

void Music::resume() {
    if (!(flags_ & kPaused)) return;
    mixer_.pause_music(false);
    flags_ &= ~kPaused;
}

// No Regret also forgets the current track (DS:91A4 = 0xFFFF).
// @orig 1210:02DF
void Music::stop() {
    if (!remorse_) current_ = -1;
    if (!(flags_ & kPlaying)) return;
    flags_ = 0;
    mixer_.stop_music();
}

// @orig 1210:01A2
void Music::play_regret(int track, int player_map) {
    const bool map_music = track == kMapMusic;
    if (map_music) track = map_track(player_map);
    if (track == current_) return;
    start(track, map_music && christmas_allowed_ && christmas() ? std::optional<std::string>("xmas") : file_name(track));
}

// No Remorse's I_playMusic (1200:0143), and the play of both games' menu,
// credits and saved track.
// @orig 1210:01A2
void Music::play(int track) {
    if (!remorse_) {
        if (track == current_) return;
        start(track, file_name(track));
        return;
    }
    stop();
    if (track == kM16) m16_ = m16_ == 'c' ? 'a' : static_cast<char>(m16_ + 1);
    start(track, file_name(track));
}

// @orig 1210:01A2
void Music::start(int track, const std::optional<std::string>& name) {
    stop();
    // The original builds the path whatever the file; a missing one is
    // silence here.
    std::optional<AmfModule> module;
    if (name) module = AmfModule::load(find_module(dir_, *name));
    if (!module) {
        // No Remorse: silence, track 0. No Regret: a track without file
        // becomes the current one, a file that does not load changes nothing.
        flags_ = 0;
        if (remorse_) current_ = 0;
        else if (!name) current_ = track;
        return;
    }
    pause();
    mixer_.play_music(std::make_shared<const AmfModule>(std::move(*module)));
    flags_ |= kPlaying;
    current_ = track;
    resume();
}

void Music::push(int track) {
    if (current_ != 0 && depth_ < static_cast<int>(stack_.size())) stack_[static_cast<std::size_t>(depth_++)] = current_;
    play(track);
}

void Music::pop() {
    if (depth_ == 0) return;
    play(stack_[static_cast<std::size_t>(--depth_)]);
}

void Music::reset() {
    depth_ = 0;
    play(0);
}

void Music::restore(int track, int flags) {
    current_ = track;
    flags_ = flags;
    play(track);
}

}  // namespace crusader
