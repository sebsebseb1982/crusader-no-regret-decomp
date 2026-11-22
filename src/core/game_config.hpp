#pragma once
// CRUSADER.CFG, "key=value" lines: read at start-up (1020:0000) and written
// back by config_save (1020:0DB7) when the options screen closes. Keys and
// values are compared in lower case.
//
// Only the keys of the options screen, the movies' and the violence are
// decoded here; the others (sound card, CD letter, mouse buttons, joystick...)
// are kept as they are. Defaults are the data segment's initial values, used
// when a key is missing.
//
// The original rewrites the whole file from its globals, with lines ending in
// "\r\n"; the port replaces the lines of the keys it knows, keeps the other
// lines and their order.

#include "core/pad_config.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace crusader {

class GameConfig {
public:
    int sound_volume = 0x40;   // "soundvolume" (the mixer's), 0..0x40
    int music_volume = 0x20;   // "musicvolume" (the mixer's)
    bool filter = true;        // "filter" (DS:5579): the mixer's oversampling
    // "dma": the Sound Blaster's DMA channel, handed to _ASS_Init (the DLL's
    // DS:EB2C). No Regret's DLL mixes for a 16-bit card from 4 up, for an
    // 8-bit one below (asylum_mixer.hpp). Read only.
    int dma = 1;
    int animation = 0;         // "animation" (DS:0864): 0 shows the animations
    int frameskip = 0;         // "frameskip" (DS:2FC8): 0, or 3 for one frame in two
    bool limit_blasts = false; // "limitblasts" (DS:2FCA)
    int mouse_speed = 2;       // "mousespeed" (DS:00A7), 0..3
    // "bigvideo" (DS:75A2, 2 when set; "true", "1" or "yes"): the movies doubled.
    bool big_video = false;
    bool subtitles = false;    // "subtitles" (DS:030A): the cutscenes' subtitles
    // No Regret: "lowresmovies" (DS:75A6), the movies decoded without their
    // luma corrections. Always written.
    bool low_res_movies = false;
    // No Regret: "moviehicolor" (DS:0309; "true", "1" or "yes"), the movies
    // played in 16-bit colour (VESA mode 0x111) instead of through their
    // 256-colour palette.
    bool movie_hicolor = false;
    // No Regret: "violence" ("true", "1" or "yes"; character 9 of "VIOLENCE=1",
    // DS:0AE1, always written). Read by UCMachine::I_true 0x06D (1058:00A1):
    // the burning, melting and blown-apart deaths, the death cries. The
    // German executable's string is "VIOLENCE=0": without the key, the German
    // data plays without (Game sets it; its installer writes the key).
    bool violence = true;
    // No Remorse's "noweasel" (no movies in Weasel's shop): No Regret neither
    // reads nor writes it, so it stays false.
    bool no_weasel = false;
    // Port addition, written only when set: "followplayer", the camera keeps
    // the controlled NPC at the centre of the screen.
    bool follow_player = false;
    // Port addition, written only when set: "directionalmove", the controller's
    // left stick walks the NPC in the direction it points (the keypad's screen
    // directions) instead of the original's tank controls.
    bool directional_move = false;
    // Port addition, written only when set: "directionalaim", the right stick
    // puts the crosshair in the direction it points.
    bool directional_aim = false;
    // Port addition, written when switched off: "drawonaim", pushing the
    // directional aim stick draws the weapon when it is holstered, without the
    // fire key first.
    bool draw_on_aim = true;
    // Port addition, written only when set: "preciseaim", the controlled NPC's
    // shots only lock on a target they can reach (a line of fire from the
    // muzzle), so an enemy one floor up or down, behind the slab, no longer
    // takes the shot from the one in sight; the target marker then always
    // shows the target the shot goes to.
    bool precise_aim = false;
    // Port addition, written when switched off: "nativeresolution", the view
    // takes the whole screen (more scenery around the camera) instead of the
    // original 640 x 480.
    bool native_resolution = true;
    // Port addition, written when switched off: "autosave", the game saves
    // itself (slot Game::kAutoSave) at the key moments of a mission.
    bool autosave = true;
    // Port addition, written only when set: "togglerun", the Shift key switches
    // running on and off at each press instead of running while it is held.
    bool toggle_run = false;
    // Port addition, written only when set: "impactmarks", shots leave marks
    // on the scenery they hit (holes, burns, scorches; damage_marks.hpp).
    bool impact_marks = false;
    // Port addition, written only when set: "bloodmarks", the wounded bleed
    // on the scenery (damage_marks.hpp) while the violence option is on.
    bool blood_marks = false;
    // Port addition, written only when set: "goremode", with the blood, much
    // more of it (DamageMarks::bleed).
    bool gore_mode = false;
    // Port addition, written only when set: "casings", firearms eject
    // casings that stay on the floor (casings.hpp).
    bool casings = false;
    // Port addition, written only when set: "barrelleaks", the explosive
    // barrels take more bullets, each pierces them, their fuel runs out into a
    // pool that catches fire when they blow up and burns whoever stands in it
    // (barrel_leaks.hpp).
    bool barrel_leaks = false;
    // Port addition, written only when set: "glassdebris", a glass wall's
    // debris fly the way the shot that broke it went, and blasts blow them
    // away (glass_debris.hpp).
    bool glass_debris = false;
    // Port addition, a cheat written only when set: "remainsloot", an enemy
    // burnt, blown apart, melted or frozen drops beside its remains the loot
    // its body would have held (death_loot.hpp); the original destroys it.
    bool remains_loot = false;
    // Port addition, written only when set: "language", the language picked
    // (language.hpp): the name of a folder of the languages folder, in lower
    // case ("fr", "en", "pt-br"...); empty for AUTO (the data's).
    std::string language;
    // Port addition, written when not 0: "viewzoom", the largest pixel size of
    // the native view, in hundredths of a screen pixel per game pixel (100 = x1,
    // 150 = x1.5, 200 = x2...); written as "1", "1.5", "2". 0 takes the biggest
    // whole multiple that fits the window, 100 gives the finest picture. A
    // fractional size stretches unevenly (some game pixels are wider), but fits
    // windows that are not a whole multiple of 640 x 480 (1360 x 768...).
    int view_zoom_pct = 0;
    // Port addition, written only when set: "smoothscaling", the picture is
    // stretched to the window with bilinear filtering (soft, as DOSBox's
    // "output=opengl") instead of keeping sharp pixels. Only visible when the
    // game pixels are bigger than the screen's (zoom above x1, or the 640 x 480
    // view in a bigger window).
    bool smooth_scaling = false;
    // Port addition, written only when set: "lighting", the world is lit in
    // 3D (engine/world/lighting.hpp): lamps, screens, flashes and explosions
    // light the scene around them, with shadows.
    bool lighting = false;
    // Port addition, written when off: "volumetricsmoke", with the 3D
    // lighting, explosions and shots leave smoke and liquids steam, volumes
    // lit by the scene.
    bool volumetric_smoke = true;
    // Port addition, written when off: "flashlight", with the 3D lighting,
    // the Silencer carries a lamp that lights in front of him. L (or the
    // controller's FLASHLIGHT action) switches it in play.
    bool flashlight = true;
    // Port addition, written when not 85: "ambientlight", with the 3D
    // lighting, the light the scene keeps where no lamp reaches, in percent
    // (0: only the lamps light it, 100: the scene is not darkened at all).
    int ambient_light = 85;
    // Port addition, written when changed: controller rumble and its strength
    // (0..100 percent).
    bool vibration = true;
    int vibration_intensity = 100;
    // Not saved: the biggest pixel size the window allows, in hundredths (set
    // by the platform layer), where the zoom choices stop.
    int view_zoom_limit_pct = 400;
    // The zoom choices after `current` (0 = auto, then x1, x1.5, x2, x3, x4),
    // the ones above `limit_pct` left out; back to 0 (auto) after the last.
    static int next_view_zoom(int current_pct, int limit_pct);
    // Port addition, written only when changed: "padmap" (comma-separated
    // action numbers, see PadButton), "padaim" and "paddeadzone".
    PadSettings pad;

    // A missing or unreadable file gives the defaults.
    static GameConfig load(const std::filesystem::path& path);
    static GameConfig parse(std::string_view text);

    std::string text() const;
    // The file has a line for `key` (in lower case).
    bool has(std::string_view key) const;
    // Throws std::runtime_error when the file cannot be written.
    void save(const std::filesystem::path& path) const;

private:
    void set(std::string_view key, const std::string& value);
    void erase(std::string_view key);

    std::vector<std::string> lines_;  // as read, without line ends
};

}  // namespace crusader
