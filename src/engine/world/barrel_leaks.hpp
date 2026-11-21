#pragma once
// Leaking barrels (port addition, the original has nothing like it;
// Enhancements screen, GameConfig::barrel_leaks). The explosive barrels
// (BARRELUP 0xAA upright, BARRELS..BARRELSE 0x151..0x155 lying, in both
// games) take several bullets before they blow up: their class counts the
// damage of each hit (BARREL, ITEM_066B) and explodes at 100 in No Remorse,
// 150 in No Regret. With the option, bullets do them less damage
// (barrel_bullet_damage), and every bullet before the last pierces the
// barrel and its fuel runs out of the hole:
//
// - A hole below the fuel's level leaks: a jet out of the barrel's side,
//   strong while the barrel is full, weaker as the level falls, at last a
//   dribble down the side, and nothing once the level is under the hole. A
//   hole in the top lets nothing out. Every hole lowers the level.
// - The fuel makes a pool on the floor where the jet lands and at the foot
//   of the barrel, growing with what has run out, flowing together; dark,
//   with a sheen. It lies on the floors at the barrel's level (none down a
//   ledge, none under what stands on the floor).
// - When the barrel explodes (the last bullet, or any blast), the pool and
//   what was left in the barrel catch fire: the fire spreads from the
//   barrel over the pool, burns a few seconds (longer the bigger the pool),
//   dies down from its edge and leaves a burnt stain. A blast or an energy
//   shot (laser, plasma, fire...) that ends on a pool sets it alight too,
//   and a burning pool lights the pools it touches.
// - The fire is a sheet of flames drawn pixel by pixel: a bed of fire over
//   the pool, tongues licking up from it, rising noise in fire colours, in
//   the game's draw order by patches of the pool.
// - Whoever stands in the fire, the Silencer included, catches fire: flames
//   over their body, and damage of fire type 0xC (the game's own fire: a
//   human it kills burns and runs) every kBurnEvery ticks, until kAfterBurn
//   ticks after they left it (burns_due: the game applies them).
// - With the 3D lighting, the fire lights the scene, its flames glow and
//   give off a dark smoke.
//
// Only bullets pierce (the fire types that leave a bullet hole or eject a
// casing). Leaks use their own numbers, never the game's random generators;
// only the damage of the fire and of the bullets on the barrels changes the
// game. They are not saved: a load or a map change forgets them.

#include "core/palette.hpp"
#include "core/surface.hpp"
#include "engine/world/lighting.hpp"
#include "engine/world/render.hpp"
#include "engine/world/world.hpp"

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace crusader {

// An explosive barrel (BARRELUP, BARRELS..BARRELSE).
bool is_fuel_barrel(std::uint16_t shape);

// The damage a shot of `fire_type` does to `shape` with the option on: a
// bullet does a fuel barrel 3/5 of it (at least 1), so that it takes about
// 1.7 times as many before it blows up; blasts and the rest are unchanged.
int barrel_bullet_damage(std::uint16_t shape, int damage, int fire_type);

class BarrelLeaks {
public:
    static constexpr std::size_t kMaxBarrels = 32;
    static constexpr std::size_t kMaxHoles = 8;  // per barrel
    static constexpr std::size_t kMaxPools = 48;
    static constexpr std::uint32_t kLeakDelay = 6;    // ticks between a hit and its leak
    static constexpr int kGrid = 4;                   // a fuel cell, usecode units
    static constexpr int kPatch = 8;                  // cells a side of a patch of fire
    static constexpr std::uint32_t kBurnEvery = 15;   // ticks between two burns
    static constexpr std::uint32_t kAfterBurn = 60;   // ticks one burns on after leaving the fire
    static constexpr int kBurnDamage = 5;             // fire type 0xC's damage
    static constexpr int kBurnFireType = 0xC;

    struct Hole {
        double ux = 0, uy = 0;     // outwards, level, unit
        double x = 0, y = 0;       // on the barrel's side, from its anchor (usecode units)
        double height = 0;         // above the barrel's bottom
        std::uint32_t made = 0;    // game tick of the hit
        bool seen = false;         // on a side the camera sees
        bool started = false;      // has leaked
        double spilled = 0;        // level units run out of it
        std::uint32_t seed = 0;
        int land_lobe = -1, foot_lobe = -1;  // its lobes in the pool
    };

    struct Barrel {
        ObjId id = 0;
        std::uint16_t shape = 0, frame = 0;
        std::int32_t x = 0, y = 0, z = 0;  // anchor when last seen
        std::int32_t floor = 0;            // the top of the floor under it, the pool's level
        double length_x = 0, length_y = 0, height = 0;  // box, usecode units
        double radius = 0;                 // cross-section
        double level = 0;                  // fuel's height above the bottom
        std::vector<Hole> holes;
        std::uint32_t pool = 0;            // Pool::serial
    };

    struct Lobe {
        double x = 0, y = 0, r = 0;  // usecode units
    };

    struct Pool {
        std::uint32_t serial = 0;
        std::int32_t z = 0;  // the floor's top
        std::vector<Lobe> lobes;
        std::uint32_t seed = 0;
        bool burning = false, burnt = false;
        std::uint32_t lit_at = 0, burn_ticks = 0;
        double fire_x = 0, fire_y = 0;  // where it caught
        // Set when it catches: the fuel on kGrid cells from (grid_x, grid_y),
        // its depth (0 edge .. 1), -1 where none lies on the floor.
        double grid_x = 0, grid_y = 0;
        int grid_w = 0, grid_h = 0;
        std::vector<float> fuel;
        std::vector<std::pair<int, int>> patches;  // first cells of the patches with fuel
    };

    // Someone on fire.
    struct Burning {
        ObjId id = 0;
        std::uint32_t until = 0;  // burns till this tick
        std::uint32_t next = 0;   // next burn
    };

    // A shot of `fire_type` ended at `at` (usecode units) on `hit` at game
    // tick `now` (WorldHooks::shot_landed): a bullet in a barrel pierces it;
    // a blast or an energy shot on a pool sets it alight.
    void shot(const World& world, ObjId hit, World::Point at, int fire_type, std::uint32_t now);

    // Runs the leaks and fires on to game tick `now`: the fuel runs out, the
    // pools grow, a barrel gone (exploded) sets its pool alight, fires burn
    // out, whoever stands in one catches fire. A tick earlier than the last
    // (a load) forgets everything.
    void advance(const World& world, std::uint32_t now);
    // The burns due since the last call, one per burn (the NPC).
    std::vector<ObjId> take_burns();

    // Once per frame, before draw: the colours of the game palette, where
    // each patch of fire comes in the draw order of `list`, the view's corner.
    void prepare(const World& world, const Palette& game_palette, const std::vector<DrawItem>& list,
                 int view_left, int view_top, int width, int height);
    // The fire drawn before every item of the list.
    void draw_first(Surface8& screen);
    // Over the item just drawn, `list[index]`: the pools on a floor, the
    // holes and the jets of a barrel, the flames on someone burning, the
    // patches of fire that come after it.
    void draw(Surface8& screen, const DrawItem& d, std::size_t index);

    // The fires' lights (lighting space), at the tick of the last advance.
    std::vector<PointLight> lights(const World& world) const;
    // Screen pixels (index, colour) of the fire drawn this frame: they glow
    // (WorldLighting::glow).
    const std::vector<std::pair<std::uint32_t, std::uint8_t>>& glowing() const { return glowing_; }
    // Where the fires give off a puff of smoke (lighting space) after game
    // tick `since` up to the last advance.
    std::vector<Vec3> smoke_since(std::uint32_t since) const;

    void clear();
    const std::vector<Barrel>& barrels() const { return barrels_; }
    const std::vector<Pool>& pools() const { return pools_; }
    const std::vector<Burning>& burning() const { return burning_; }
    const Pool* pool(std::uint32_t serial) const;
    bool on_fire(ObjId id) const;

    // How much fuel there is at (x, y) of pool `p`: 1 on its edge, more
    // inside (metaballs of its lobes), with a ragged edge.
    static double field(const Pool& p, double x, double y);
    // How strongly pool `p` burns at (x, y) at game tick `tick`: 0 where the
    // fire has not come or has gone out, up to 1.
    double fire_at(const Pool& p, double x, double y, std::uint32_t tick) const;

private:
    enum Tone : std::uint8_t {
        kRim, kBody, kDeep, kSheen, kStream, kStreamLight, kHole, kWet, kHot, kAsh, kAshDark,
        kFlameWhite, kFlameYellow, kFlameOrange, kFlameDeep, kFlameRed, kFlameDark, kFlameSmoke, kTones
    };

    Pool* pool_of(std::uint32_t serial);
    void step(const World& world, std::uint32_t tick);
    void ignite(const World& world, Pool& p, double x, double y, std::uint32_t tick);
    void explode(const World& world, Barrel& b, std::uint32_t tick);
    void grow(Barrel& b);
    void catch_fire(const World& world, const Pool& p, std::uint32_t tick);
    bool opaque(const DrawItem& d, int sx, int sy) const;
    void draw_pool(Surface8& screen, const DrawItem& d, const Pool& p);
    void draw_barrel(Surface8& screen, const DrawItem& d, const Barrel& b);
    void draw_patch(Surface8& screen, const Pool& p, std::pair<int, int> patch);
    void draw_person(Surface8& screen, const DrawItem& d, const Burning& b);
    // One column of fire at screen x `px`: `base[k]` is how strongly the
    // row `bottom - k` burns; flames rise from it up to `height` pixels.
    void fire_column(Surface8& screen, int px, int bottom, const std::vector<float>& base, float height,
                     std::uint32_t seed);
    void put(Surface8& screen, int x, int y, Tone tone, bool glows = false);
    float fire_strength(const Pool& p, std::uint32_t tick) const;

    std::vector<Barrel> barrels_;
    std::vector<Pool> pools_;
    std::vector<Burning> burning_;
    std::vector<ObjId> burns_;
    int map_ = -1;
    std::uint32_t now_ = 0;
    bool started_ = false;
    std::uint32_t next_serial_ = 1;
    std::uint32_t next_seed_ = 0x6A09E667u;

    // Per frame.
    int view_left_ = 0, view_top_ = 0;
    std::array<std::array<std::uint8_t, 256>, kTones> tone_{};
    Palette tone_palette_{};
    bool tone_ready_ = false;
    // The patches (pool, patch) to draw after the item of the list at an index.
    std::unordered_map<std::size_t, std::vector<std::pair<std::size_t, std::size_t>>> fire_after_;
    std::vector<std::pair<std::size_t, std::size_t>> fire_first_;
    std::vector<std::pair<std::uint32_t, std::uint8_t>> glowing_;
    // The fire already drawn this frame at each pixel: how hot, its colour.
    int width_ = 0;
    std::uint32_t frame_ = 0;
    std::vector<std::uint32_t> heat_frame_;
    std::vector<float> heat_;
    std::vector<std::uint8_t> heat_colour_;
    std::vector<float> column_;
    mutable std::unordered_map<const ShapeFrame*, std::vector<std::uint8_t>> masks_;
};

}  // namespace crusader
