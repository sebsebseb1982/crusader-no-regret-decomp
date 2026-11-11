#pragma once
// No Regret's movie player (XANPLAY.C, segment 1448): usecode intrinsic 0x06D
// MovieGump::I_playMovieCutsceneRegret(name, fade) (1448:0B5A), also called
// by Game_start (1030:032D) for ORIGIN then ANIM01, and by the video test of
// the cheats (ORIGIN then ANIM01 until a key stops them). The game is frozen
// meanwhile.
//
// 1448:0B5A:
//   - the file is the "flics" directory's <name>.AVI, with a '0' before a
//     name shorter than 3 characters ("1a" -> "01a"); nothing happens when
//     it does not exist. With "subtitles=true", <name>.IFF holds the
//     subtitles (movie_subtitles.hpp);
//   - "bigvideo=true" (DS:75A2) doubles the movie, except ORIGIN;
//   - the colour cycling is paused (DS:73E0 +0x40), the cursor hidden
//     (WORLD_1008_0F68(0)), the screen cleared; then 1448:06F4 plays it;
//   - at the end: the screen cleared; a fade argument of 0 jumps to an all
//     black palette (PaletteFaderProcess::I_jumpToAllBlack), another one
//     writes the game palette back; the cycling resumes in both cases.
//
// 1448:06F4 opens the stream, creates the decoder (XANDRV32 mode 3, the
// movie's BEST table) and sets the palette (1448:03B4): PAL8, its components
// shifted down to 6 bits (version 4) or masked to them (version 3); the
// table's 0xFF entries become the colour nearest to colour 255, which turns
// white (63, 63, 63, DS:75A8, for the subtitles); the darkest colour
// (DS:75A9) fills the screen. The movie is centred, doubled or not. The
// keyboard is flushed (11E8:33E3) before and after.
//
// Each frame (1448:01BA, called by the audio-clocked loop 1468:04C8): the
// frame is decoded and drawn; a frame that is late is decoded without its
// corrections and not drawn ("lowresmovies=true", DS:75A6, never decodes
// them). Then the subtitle of this frame, if any (1440:0732: entries by frame
// number, from 0), is drawn (1440:036C): a 640 x 15 strip filled with colour
// 1, the text in font 3 centred at y 10, then each pixel 0 white and the
// others the darkest colour, copied to y 420 (1440:0570). A key pressed (the
// keyboard buffer not empty) stops the movie after the frame.
//
// Not reproduced: the disk streaming (the whole file is read at once); the
// 16-bit colour mode ("moviehicolor=true", DS:0309, XANDRV32 modes 2 and 4);
// the loop waiting for running fades before the movie starts; the text drawn
// then erased by 1448:0000 (it caches the font's glyphs). The sound is one
// buffer of every audio chunk, the frames follow real time at 15 a second
// (the original follows the sound card).

#include "core/font.hpp"
#include "core/movie_subtitles.hpp"
#include "core/palette.hpp"
#include "core/surface.hpp"
#include "core/xan_movie.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace crusader {

// 1448:0B5A: the file name without extension, upper case (the port's data
// is upper case), with a leading '0' when shorter than 3 characters.
std::string regret_movie_base_name(std::string_view name);

class XanPlayer {
public:
    static constexpr int kScreenWidth = 640, kScreenHeight = 480;
    static constexpr int kSubtitleFont = 3;
    static constexpr int kSubtitleHeight = 15, kSubtitleTextY = 10, kSubtitleY = 420;
    static constexpr std::uint8_t kSubtitleBackground = 1;
    static constexpr std::uint8_t kWhite = 0xFF;

    // `big`: "bigvideo=true" and not ORIGIN. `fade`: the intrinsic's second
    // argument.
    XanPlayer(XanMovie movie, std::vector<RegretSubtitle> subtitles, bool big, bool low_res, bool fade,
              const FontSet& fonts);

    // Real time; `key_pressed`: a key went down since the last call.
    void advance(std::uint32_t elapsed_us, bool key_pressed);

    bool finished() const { return finished_; }
    bool fade() const { return fade_; }
    const Surface8& screen() const { return screen_; }
    const Palette& palette() const { return palette_; }
    // Frames decoded so far.
    int frame() const { return frame_; }
    // The darkest colour (DS:75A9).
    std::uint8_t darkest() const { return darkest_; }

    // The movie's sound: signed 8-bit mono at XanMovie::kAudioRate.
    // take_sound_start() is true once, when it should start.
    const std::vector<std::int8_t>& sound() const { return sound_; }
    bool take_sound_start();

private:
    void set_palette();
    void next_frame(bool late);
    void show_subtitle(const std::string& text);

    XanMovie movie_;
    XanDecoder decoder_;
    std::vector<RegretSubtitle> subtitles_;
    bool doubled_;
    bool low_res_;
    bool fade_;
    const FontSet* fonts_;
    Surface8 screen_;
    Palette palette_;
    std::uint8_t darkest_ = 0;
    int x_ = 0, y_ = 0;

    std::size_t chunk_ = 0;
    int frame_ = 0;
    std::size_t subtitle_ = 0;  // DS:759E
    std::uint64_t elapsed_us_ = 0;
    bool started_ = false;
    bool finished_ = false;
    std::vector<std::int8_t> sound_;
    bool sound_due_ = false;
};

}  // namespace crusader
