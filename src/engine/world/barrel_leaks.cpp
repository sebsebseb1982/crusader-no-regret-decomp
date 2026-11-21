#include "engine/world/barrel_leaks.hpp"

#include "core/dtable.hpp"
#include "engine/world/casings.hpp"
#include "engine/world/damage_marks.hpp"

#include <algorithm>
#include <cmath>

namespace crusader {

namespace {

constexpr std::uint16_t kItemExploded = 0x0040;  // SPRITE_1150_0891 ran (item_damage.cpp)
constexpr double kPi = 3.14159265358979;
constexpr double kGravity = 0.25;     // usecode units a tick per tick, the jet's fall
constexpr double kJet = 0.65;         // jet speed, units a tick, per square root of the head
constexpr double kDrain = 0.006;      // level fall a tick per hole, per square root of the head
constexpr double kFull = 0.9;         // a whole barrel's level, of its height
constexpr double kThickness = 0.7;    // a pool's depth: its area is the fuel run out over it
constexpr double kMaxLand = 150, kMaxFoot = 60, kMaxLeft = 110;  // lobe radii, usecode units
constexpr double kDribble = 0.5;      // below this jet speed, the fuel runs down the side
constexpr double kSpread = 2.5;       // the fire's spread over a pool, units a tick
constexpr float kFlameHeight = 64;    // pixels, the tallest flames
constexpr int kHeight = 2;            // a floor this close to a pool's level takes it
constexpr int kPad = 256;             // largest footpad reach of an anchor

// The leaks' own numbers (splitmix32), never the game's generators.
// @orig none: port-only barrel leaks
std::uint32_t mix(std::uint32_t v) {
    v += 0x9E3779B9u;
    v = (v ^ (v >> 16)) * 0x85EBCA6Bu;
    v = (v ^ (v >> 13)) * 0xC2B2AE35u;
    return v ^ (v >> 16);
}

std::uint32_t mix(std::uint32_t seed, int a, int b) {
    return mix(seed ^ mix(static_cast<std::uint32_t>(a) * 0x27D4EB2Fu ^ static_cast<std::uint32_t>(b)));
}

double unit(std::uint32_t h) { return (h & 0xFFFF) / 65535.0; }

// Value noise, 0..1, features of one unit.
// @orig none: port-only barrel leaks
double noise(double x, double y, std::uint32_t seed) {
    const double fx = std::floor(x), fy = std::floor(y);
    const int ix = static_cast<int>(fx), iy = static_cast<int>(fy);
    double tx = x - fx, ty = y - fy;
    tx = tx * tx * (3 - 2 * tx);
    ty = ty * ty * (3 - 2 * ty);
    const double a = unit(mix(seed, ix, iy)), b = unit(mix(seed, ix + 1, iy));
    const double c = unit(mix(seed, ix, iy + 1)), d = unit(mix(seed, ix + 1, iy + 1));
    return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}

// The same, faster (a table of 256 values), for the fire's pixels.
// @orig none: port-only barrel leaks
float fast_noise(float x, float y, std::uint32_t seed) {
    static const auto table = [] {
        std::array<float, 256> t{};
        std::array<std::uint8_t, 256> perm{};
        for (int i = 0; i < 256; ++i) {
            t[i] = static_cast<float>(unit(mix(static_cast<std::uint32_t>(i) * 0x9E3779B9u + 1)));
            perm[i] = static_cast<std::uint8_t>(mix(static_cast<std::uint32_t>(i) + 77) & 0xFF);
        }
        return std::pair{t, perm};
    }();
    const float fx = std::floor(x), fy = std::floor(y);
    const int ix = static_cast<int>(fx), iy = static_cast<int>(fy);
    float tx = x - fx, ty = y - fy;
    tx = tx * tx * (3 - 2 * tx);
    ty = ty * ty * (3 - 2 * ty);
    const auto at = [&](int i, int j) {
        return table.first[table.second[(table.second[(i + static_cast<int>(seed)) & 0xFF] + j) & 0xFF]];
    };
    const float a = at(ix, iy), b = at(ix + 1, iy), c = at(ix, iy + 1), d = at(ix + 1, iy + 1);
    const float top = a + (b - a) * tx, bottom = c + (d - c) * tx;
    return top + (bottom - top) * ty;
}

int screen_x(double x, double y) { return static_cast<int>(std::floor((x - y) / 2)); }
int screen_y(double x, double y, double z) { return static_cast<int>(std::floor((x + y) / 4 - z)); }

bool upright(std::uint16_t shape) { return shape == 0xAA; }

// The pool's bounds, usecode units.
struct Bounds {
    double x0 = 0, y0 = 0, x1 = -1, y1 = -1;
};

// @orig none: port-only barrel leaks
Bounds bounds_of(const BarrelLeaks::Pool& p) {
    Bounds b;
    bool any = false;
    for (const BarrelLeaks::Lobe& l : p.lobes) {
        if (l.r <= 0) continue;
        const double reach = l.r * 1.6 + 2;
        if (!any) b = {l.x - reach, l.y - reach, l.x + reach, l.y + reach};
        b.x0 = std::min(b.x0, l.x - reach);
        b.y0 = std::min(b.y0, l.y - reach);
        b.x1 = std::max(b.x1, l.x + reach);
        b.y1 = std::max(b.y1, l.y + reach);
        any = true;
    }
    return b;
}

double depth_of(double field) { return std::clamp((field - 1) / 2, 0.0, 1.0); }

// Scenery around: what a pool lies on or is stopped by.
struct Solid {
    std::int32_t x_left, x, y_far, y, z, z_top;
};

// @orig none: port-only barrel leaks
std::vector<Solid> solids_around(const World& world, std::int32_t x0, std::int32_t y0, std::int32_t x1,
                                 std::int32_t y1) {
    std::vector<Solid> out;
    for (ObjId id : world.rect_search(x0 - kPad, y0 - kPad, x1 + kPad, y1 + kPad)) {
        const Item* item = world.get(id);
        if (!item || is_npc_id(item->id) || item->parent != 0 || item->off_map || item->sprite_only ||
            is_fuel_barrel(item->shape) || (item->flags & (kItemInvisible | kItemContained | kItemEthereal)))
            continue;
        const ShapeInfo* info = world.shape_info(item->shape);
        if (!info || info->is(ShapeInfo::kEditor) || info->is(ShapeInfo::kTranslucent)) continue;
        const bool mirrored = item->flags & kItemFlipped;
        out.push_back({item->x - (mirrored ? info->footpad_y : info->footpad_x) * 16, item->x,
                       item->y - (mirrored ? info->footpad_x : info->footpad_y) * 16, item->y, item->z,
                       item->z + info->footpad_z * 8});
    }
    return out;
}

// The top of the floor under (x, y): the highest scenery whose box holds it
// with its top at most 16 below z (a barrel stands a little above the flat
// floor tiles); z itself when there is none.
// @orig none: port-only barrel leaks
std::int32_t floor_top(const std::vector<Solid>& solids, std::int32_t x, std::int32_t y, std::int32_t z) {
    std::int32_t top = z;
    bool found = false;
    for (const Solid& s : solids) {
        if (x < s.x_left || x > s.x || y < s.y_far || y > s.y || s.z_top > z + kHeight || s.z_top < z - 16) continue;
        if (!found || s.z_top > top) top = s.z_top;
        found = true;
    }
    return top;
}

// A pool at level z lies at (x, y): a floor there at that level, nothing
// standing on it (the barrel that leaked aside).
// @orig none: port-only barrel leaks
bool on_floor(const std::vector<Solid>& solids, double x, double y, std::int32_t z) {
    const auto ix = static_cast<std::int32_t>(std::lround(x)), iy = static_cast<std::int32_t>(std::lround(y));
    bool floor = false;
    for (const Solid& s : solids) {
        if (ix < s.x_left || ix > s.x || iy < s.y_far || iy > s.y) continue;
        if (std::abs(s.z_top - z) <= kHeight) floor = true;
        else if (s.z_top > z + kHeight && s.z <= z + 16) return false;  // standing there
    }
    return floor;
}

}  // namespace

// @orig none: port-only barrel leaks
bool is_fuel_barrel(std::uint16_t shape) { return shape == 0xAA || (shape >= 0x151 && shape <= 0x155); }

// @orig none: port-only barrel leaks
int barrel_bullet_damage(std::uint16_t shape, int damage, int fire_type) {
    const bool bullet = mark_kind(fire_type) == MarkKind::Bullet || ejection_of(fire_type) != Ejection::None;
    if (!is_fuel_barrel(shape) || !bullet) return damage;
    return std::max(1, damage * 3 / 5);
}

// @orig none: port-only barrel leaks
double BarrelLeaks::field(const Pool& p, double x, double y) {
    double s = 0;
    for (const Lobe& l : p.lobes) {
        if (l.r <= 0) continue;
        const double dx = x - l.x, dy = y - l.y;
        s += l.r * l.r / (dx * dx + dy * dy + 1);
    }
    // A ragged edge: the fuel runs further here and there.
    return s * (0.82 + 0.36 * noise(x / 7, y / 7, p.seed));
}

const BarrelLeaks::Pool* BarrelLeaks::pool(std::uint32_t serial) const {
    for (const Pool& p : pools_)
        if (p.serial == serial) return &p;
    return nullptr;
}

BarrelLeaks::Pool* BarrelLeaks::pool_of(std::uint32_t serial) {
    for (Pool& p : pools_)
        if (p.serial == serial) return &p;
    return nullptr;
}

bool BarrelLeaks::on_fire(ObjId id) const {
    return std::any_of(burning_.begin(), burning_.end(), [&](const Burning& b) { return b.id == id; });
}

// @orig none: port-only barrel leaks
void BarrelLeaks::shot(const World& world, ObjId hit, World::Point at, int fire_type, std::uint32_t now) {
    if (world.current_map() != map_) {
        clear();
        map_ = world.current_map();
    }
    const MarkKind kind = mark_kind(fire_type);
    const bool bullet = kind == MarkKind::Bullet || ejection_of(fire_type) != Ejection::None;
    if (kind == MarkKind::Scorch || (kind == MarkKind::Burn && !bullet)) {
        // A blast within reach of a pool, an energy shot on one: it catches.
        const dtable::FireType* ft = dtable::fire_type(fire_type);
        const double reach = kind == MarkKind::Scorch ? (ft ? ft->splash() : 0) * 16.0 + 8 : 0;
        for (Pool& p : pools_) {
            if (p.burning || p.burnt || std::abs(at.z - p.z) > (kind == MarkKind::Scorch ? 48 : 16)) continue;
            bool near = field(p, at.x, at.y) >= 1;
            for (const Lobe& l : p.lobes) {
                const double dx = at.x - l.x, dy = at.y - l.y;
                near = near || (reach > 0 && l.r > 0 && std::sqrt(dx * dx + dy * dy) <= reach + l.r);
            }
            if (near) ignite(world, p, at.x, at.y, now);
        }
        if (kind == MarkKind::Scorch) return;
    }
    if (!bullet) return;
    const Item* item = world.get(hit);
    if (!item || !is_fuel_barrel(item->shape) || (item->flags & kItemExploded)) return;
    const ShapeInfo* info = world.shape_info(item->shape);
    if (!info) return;
    auto found = std::find_if(barrels_.begin(), barrels_.end(),
                              [&](const Barrel& b) { return b.id == hit && b.shape == item->shape; });
    if (found == barrels_.end()) {
        if (barrels_.size() >= kMaxBarrels) barrels_.erase(barrels_.begin());
        Barrel b;
        b.id = hit;
        b.shape = item->shape;
        b.frame = item->frame;
        b.x = item->x;
        b.y = item->y;
        b.z = item->z;
        const bool mirrored = item->flags & kItemFlipped;
        b.length_x = (mirrored ? info->footpad_y : info->footpad_x) * 16.0;
        b.length_y = (mirrored ? info->footpad_x : info->footpad_y) * 16.0;
        b.height = info->footpad_z * 8.0;
        b.radius = upright(b.shape) ? std::min(b.length_x, b.length_y) / 2 : b.height / 2;
        b.level = b.height * kFull;
        const auto cx = static_cast<std::int32_t>(b.x - b.length_x / 2);
        const auto cy = static_cast<std::int32_t>(b.y - b.length_y / 2);
        b.floor = floor_top(solids_around(world, cx, cy, cx, cy), cx, cy, b.z);
        barrels_.push_back(b);
        found = barrels_.end() - 1;
    }
    Barrel& b = *found;
    if (b.holes.size() >= kMaxHoles || b.height <= 2 || b.length_x <= 0 || b.length_y <= 0) return;
    const double cx = b.x - b.length_x / 2, cy = b.y - b.length_y / 2;
    Hole h;
    double hx = 0, hy = 0;
    if (upright(b.shape)) {
        // On the round side, across from where the shot came.
        double ux = at.x - cx, uy = at.y - cy;
        const double len = std::sqrt(ux * ux + uy * uy);
        if (len < 1) ux = uy = std::sqrt(0.5);
        else ux /= len, uy /= len;
        h.ux = ux;
        h.uy = uy;
        hx = cx + ux * b.radius;
        hy = cy + uy * b.radius;
        h.seen = ux + uy > 0.1;
    } else {
        hx = std::clamp(static_cast<double>(at.x), b.x - b.length_x, static_cast<double>(b.x));
        hy = std::clamp(static_cast<double>(at.y), b.y - b.length_y, static_cast<double>(b.y));
        double ux = hx - cx, uy = hy - cy;
        const double len = std::sqrt(ux * ux + uy * uy);
        if (len < 1) ux = uy = std::sqrt(0.5);
        else ux /= len, uy /= len;
        h.ux = ux;
        h.uy = uy;
        h.seen = hx >= b.x - 4 || hy >= b.y - 4;
    }
    h.x = hx - b.x;
    h.y = hy - b.y;
    // A shot on the top makes a hole above the fuel: it lets nothing out.
    h.height = at.z >= b.z + b.height - 1 ? b.height : std::clamp(static_cast<double>(at.z - b.z), 1.0, b.height - 1);
    h.made = now;
    next_seed_ = mix(next_seed_);
    h.seed = next_seed_;
    b.holes.push_back(h);
}

// @orig none: port-only barrel leaks
void BarrelLeaks::advance(const World& world, std::uint32_t now) {
    if (world.current_map() != map_) {
        clear();
        map_ = world.current_map();
    }
    if (started_ && now < now_) clear();  // a load: the clock went back
    if (!started_) {
        started_ = true;
        now_ = now;
    }
    std::uint32_t from = now_;
    if (now - from > 600) from = now - 600;
    for (std::uint32_t t = from + 1; t <= now && t != 0; ++t) step(world, t);
    now_ = now;
}

// @orig none: port-only barrel leaks
std::vector<ObjId> BarrelLeaks::take_burns() {
    std::vector<ObjId> out;
    out.swap(burns_);
    return out;
}

// @orig none: port-only barrel leaks
void BarrelLeaks::step(const World& world, std::uint32_t tick) {
    for (std::size_t i = 0; i < barrels_.size();) {
        Barrel& b = barrels_[i];
        const Item* item = world.get(b.id);
        if (!item || item->shape != b.shape || (item->flags & kItemExploded)) {
            explode(world, b, tick);
            barrels_.erase(barrels_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        b.x = item->x;
        b.y = item->y;
        b.z = item->z;
        b.frame = item->frame;
        for (Hole& h : b.holes) {
            if (!h.started && tick - h.made >= kLeakDelay) {
                h.started = true;
                if (b.level > h.height) {
                    // Its lobes in the barrel's pool (a new one if it burnt):
                    // where the jet lands now, and at the foot below it.
                    Pool* p = pool_of(b.pool);
                    if (!p || p->burnt) {
                        if (pools_.size() >= kMaxPools) {
                            auto old = std::find_if(pools_.begin(), pools_.end(),
                                                    [](const Pool& q) { return !q.burning; });
                            pools_.erase(old != pools_.end() ? old : pools_.begin());
                        }
                        Pool fresh;
                        fresh.serial = next_serial_++;
                        fresh.z = b.floor;
                        next_seed_ = mix(next_seed_);
                        fresh.seed = next_seed_;
                        pools_.push_back(fresh);
                        b.pool = fresh.serial;
                        for (Hole& other : b.holes) other.land_lobe = other.foot_lobe = -1, other.spilled = 0;
                        p = &pools_.back();
                    }
                    const double v = kJet * std::sqrt(b.level - h.height);
                    const double fall = std::sqrt(2 * (h.height + b.z - b.floor) / kGravity);
                    const double reach = v >= kDribble ? v * fall : 1;
                    const double hx = b.x + h.x, hy = b.y + h.y;
                    h.land_lobe = static_cast<int>(p->lobes.size());
                    p->lobes.push_back({hx + h.ux * (reach + 2), hy + h.uy * (reach + 2), 0});
                    h.foot_lobe = static_cast<int>(p->lobes.size());
                    p->lobes.push_back({hx + h.ux * 3, hy + h.uy * 3, 0});
                }
            }
            if (!h.started) continue;
            const double head = b.level - h.height;
            if (head <= 0) continue;
            const double out = std::min(head, kDrain * std::sqrt(head));
            b.level -= out;
            h.spilled += out;
        }
        grow(b);
        ++i;
    }
    for (Pool& p : pools_) {
        if (!p.burning) continue;
        if (tick - p.lit_at >= p.burn_ticks) {
            p.burning = false;
            p.burnt = true;
            continue;
        }
        catch_fire(world, p, tick);
        // A burning pool lights the pools it touches.
        if ((tick - p.lit_at) % 8 != 0 || tick - p.lit_at < 16) continue;
        for (Pool& q : pools_) {
            if (&q == &p || q.burning || q.burnt || std::abs(q.z - p.z) > 4) continue;
            for (const Lobe& a : p.lobes) {
                for (const Lobe& c : q.lobes) {
                    const double dx = a.x - c.x, dy = a.y - c.y;
                    if (a.r > 0 && c.r > 0 && std::sqrt(dx * dx + dy * dy) < a.r + c.r)
                        ignite(world, q, c.x, c.y, tick);
                }
            }
        }
    }
    // The people on fire burn, every kBurnEvery ticks, until they are out.
    for (std::size_t i = 0; i < burning_.size();) {
        Burning& b = burning_[i];
        const Item* item = world.get(b.id);
        if (!item || world.is_dead(b.id) || tick >= b.until) {
            burning_.erase(burning_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        if (tick >= b.next) {
            burns_.push_back(b.id);
            b.next = tick + kBurnEvery;
        }
        ++i;
    }
}

// Whoever stands in the fire of `p` catches fire.
// @orig none: port-only barrel leaks
void BarrelLeaks::catch_fire(const World& world, const Pool& p, std::uint32_t tick) {
    const Bounds bb = bounds_of(p);
    if (bb.x1 < bb.x0) return;
    const auto x0 = static_cast<std::int32_t>(bb.x0), y0 = static_cast<std::int32_t>(bb.y0);
    const auto x1 = static_cast<std::int32_t>(bb.x1), y1 = static_cast<std::int32_t>(bb.y1);
    for (ObjId id : world.rect_search(x0, y0, x1 + kPad, y1 + kPad)) {
        if (!is_npc_id(id)) continue;
        const Item* item = world.get(id);
        if (!item || item->off_map || item->parent != 0 || world.is_dead(id)) continue;
        if (item->z < p.z - kHeight || item->z > p.z + 16) continue;
        const ShapeInfo* info = world.shape_info(item->shape);
        const double cx = item->x - (info ? info->footpad_x : 0) * 8.0;
        const double cy = item->y - (info ? info->footpad_y : 0) * 8.0;
        // The feet: the middle and four points around it.
        double hottest = fire_at(p, cx, cy, tick);
        static constexpr int kAround[4][2] = {{6, 0}, {-6, 0}, {0, 6}, {0, -6}};
        for (const auto& a : kAround) hottest = std::max(hottest, fire_at(p, cx + a[0], cy + a[1], tick));
        if (hottest < 0.25) continue;
        auto found = std::find_if(burning_.begin(), burning_.end(), [&](const Burning& b) { return b.id == id; });
        if (found == burning_.end()) burning_.push_back({id, tick + kAfterBurn, tick + 4});
        else found->until = tick + kAfterBurn;
    }
}

// The lobes of a barrel's pool, from what ran out of each hole.
// @orig none: port-only barrel leaks
void BarrelLeaks::grow(Barrel& b) {
    Pool* p = pool_of(b.pool);
    if (!p) return;
    const double area = b.length_x * b.length_y / kPi;  // its cross-section, as a disc's radius squared
    for (const Hole& h : b.holes) {
        if (h.land_lobe < 0 || h.foot_lobe < 0 || static_cast<std::size_t>(h.foot_lobe) >= p->lobes.size()) continue;
        const double r = std::min(kMaxLand, 0.8 * std::sqrt(area * h.spilled / kThickness));
        p->lobes[static_cast<std::size_t>(h.land_lobe)].r = r;
        p->lobes[static_cast<std::size_t>(h.foot_lobe)].r = std::min(kMaxFoot, 0.55 * r);
    }
}

// The barrel blew up: what was left in it splashes where it stood, and its
// pool catches fire from there.
// @orig none: port-only barrel leaks
void BarrelLeaks::explode(const World& world, Barrel& b, std::uint32_t tick) {
    Pool* p = pool_of(b.pool);
    if (!p || p->burnt) return;
    const double cx = b.x - b.length_x / 2, cy = b.y - b.length_y / 2;
    if (b.level > 1) {
        const double area = b.length_x * b.length_y / kPi;
        p->lobes.push_back({cx, cy, std::min(kMaxLeft, 0.6 * std::sqrt(area * b.level / kThickness))});
    }
    ignite(world, *p, cx, cy, tick);
}

// @orig none: port-only barrel leaks
void BarrelLeaks::ignite(const World& world, Pool& p, double x, double y, std::uint32_t tick) {
    if (p.burning || p.burnt) return;
    double extent = 0;
    for (const Lobe& l : p.lobes) extent = std::max(extent, l.r);
    if (extent < 2) return;
    p.burning = true;
    p.lit_at = tick;
    p.fire_x = x;
    p.fire_y = y;
    p.burn_ticks = static_cast<std::uint32_t>(std::clamp(240.0 + 5 * extent, 240.0, 900.0));
    // The fuel, cell by cell, where it lies on the floor; the patches that
    // hold some.
    const Bounds bb = bounds_of(p);
    p.grid_x = std::floor(bb.x0 / kGrid) * kGrid;
    p.grid_y = std::floor(bb.y0 / kGrid) * kGrid;
    p.grid_w = static_cast<int>(std::ceil((bb.x1 - p.grid_x) / kGrid)) + 1;
    p.grid_h = static_cast<int>(std::ceil((bb.y1 - p.grid_y) / kGrid)) + 1;
    p.fuel.assign(static_cast<std::size_t>(p.grid_w) * p.grid_h, -1.0f);
    const auto solids = solids_around(world, static_cast<std::int32_t>(bb.x0), static_cast<std::int32_t>(bb.y0),
                                      static_cast<std::int32_t>(bb.x1), static_cast<std::int32_t>(bb.y1));
    p.patches.clear();
    for (int pj = 0; pj < p.grid_h; pj += kPatch) {
        for (int pi = 0; pi < p.grid_w; pi += kPatch) {
            bool any = false;
            for (int j = pj; j < std::min(pj + kPatch, p.grid_h); ++j) {
                for (int i = pi; i < std::min(pi + kPatch, p.grid_w); ++i) {
                    const double cx = p.grid_x + (i + 0.5) * kGrid, cy = p.grid_y + (j + 0.5) * kGrid;
                    const double s = field(p, cx, cy);
                    if (s < 1 || !on_floor(solids, cx, cy, p.z)) continue;
                    p.fuel[static_cast<std::size_t>(j) * p.grid_w + i] = static_cast<float>(depth_of(s));
                    any = true;
                }
            }
            if (any) p.patches.emplace_back(pi, pj);
        }
    }
}

// 0 out .. 1 at its height: it takes at once, then dies down.
// @orig none: port-only barrel leaks
float BarrelLeaks::fire_strength(const Pool& p, std::uint32_t tick) const {
    if (!p.burning || p.burn_ticks == 0) return 0;
    const float age = static_cast<float>(tick - p.lit_at);
    const float ramp = std::min(1.0f, 0.35f + age / 20);
    const float u = age / static_cast<float>(p.burn_ticks);
    const float tail = u > 0.6f ? std::max(0.0f, 1 - (u - 0.6f) / 0.4f) : 1.0f;
    return ramp * (0.35f + 0.65f * tail);
}

// Reached by the fire, not yet burnt out there: the edge goes out first.
// @orig none: port-only barrel leaks
double BarrelLeaks::fire_at(const Pool& p, double x, double y, std::uint32_t tick) const {
    if (!p.burning || p.fuel.empty()) return 0;
    const int i = static_cast<int>(std::floor((x - p.grid_x) / kGrid));
    const int j = static_cast<int>(std::floor((y - p.grid_y) / kGrid));
    if (i < 0 || j < 0 || i >= p.grid_w || j >= p.grid_h) return 0;
    const float fuel = p.fuel[static_cast<std::size_t>(j) * p.grid_w + i];
    if (fuel < 0) return 0;
    const double age = tick - p.lit_at;
    const double from = std::sqrt((x - p.fire_x) * (x - p.fire_x) + (y - p.fire_y) * (y - p.fire_y));
    const double reached = age * kSpread - from;
    if (reached <= 0) return 0;
    const double dying = std::max(0.0, (age / p.burn_ticks - 0.55) / 0.45);
    if (fuel < dying) return 0;
    return (0.5 + 0.4 * fuel) * std::min(1.0, reached / 12) * std::min(1.0, (fuel - dying) * 8 + 0.25) *
           fire_strength(p, tick);
}

// @orig none: port-only barrel leaks
void BarrelLeaks::prepare(const World& world, const Palette& game_palette, const std::vector<DrawItem>& list,
                          int view_left, int view_top, int width, int height) {
    if (world.current_map() != map_) {
        clear();
        map_ = world.current_map();
    }
    view_left_ = view_left;
    view_top_ = view_top;
    glowing_.clear();
    ++frame_;
    width_ = width;
    const std::size_t pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(std::max(height, 0));
    if (heat_frame_.size() != pixels) {
        heat_frame_.assign(pixels, 0);
        heat_.assign(pixels, 0);
        heat_colour_.assign(pixels, 0);
    }
    bool same = tone_ready_;
    for (int c = 0; same && c < 256; ++c) {
        if (c >= 1 && c <= 14) continue;
        const Rgb &a = game_palette.colors[c], &b = tone_palette_.colors[c];
        same = a.r == b.r && a.g == b.g && a.b == b.b;
    }
    if (!same) {
        // The fuel, dark and a little see-through at its edge; its jet; the
        // holes; the pool glowing under the fire; the burnt stain; the
        // fire, white at its heart, yellow, orange, red, then a see-through
        // dark red and a wisp of smoke at its tips.
        tone_[kRim] = tint_table(game_palette, {52, 36, 16}, 55);
        tone_[kBody] = tint_table(game_palette, {30, 21, 10}, 80);
        tone_[kDeep] = tint_table(game_palette, {18, 12, 6}, 88);
        tone_[kSheen] = tint_table(game_palette, {70, 60, 48}, 75);
        tone_[kStream] = tint_table(game_palette, {92, 62, 26}, 78);
        tone_[kStreamLight] = tint_table(game_palette, {196, 160, 90}, 72);
        tone_[kHole] = shade_table(game_palette, 12);
        tone_[kWet] = tint_table(game_palette, {34, 22, 10}, 45);
        tone_[kHot] = tint_table(game_palette, {150, 50, 12}, 70);
        tone_[kAsh] = shade_table(game_palette, 58);
        tone_[kAshDark] = shade_table(game_palette, 38);
        tone_[kFlameWhite] = tint_table(game_palette, {255, 248, 204}, 100);
        tone_[kFlameYellow] = tint_table(game_palette, {255, 214, 84}, 100);
        tone_[kFlameOrange] = tint_table(game_palette, {250, 150, 40}, 100);
        tone_[kFlameDeep] = tint_table(game_palette, {228, 94, 22}, 92);
        tone_[kFlameRed] = tint_table(game_palette, {176, 42, 10}, 75);
        tone_[kFlameDark] = tint_table(game_palette, {110, 24, 6}, 50);
        tone_[kFlameSmoke] = tint_table(game_palette, {36, 28, 24}, 28);
        tone_palette_ = game_palette;
        tone_ready_ = true;
    }
    // Each patch of fire right after the last item it must be drawn over (a
    // box of its own in the game's draw order), before the items in front.
    fire_after_.clear();
    fire_first_.clear();
    for (std::size_t pi = 0; pi < pools_.size(); ++pi) {
        const Pool& p = pools_[pi];
        if (!p.burning) continue;
        for (std::size_t k = 0; k < p.patches.size(); ++k) {
            const double x0 = p.grid_x + p.patches[k].first * kGrid, y0 = p.grid_y + p.patches[k].second * kGrid;
            const double x1 = x0 + kPatch * kGrid, y1 = y0 + kPatch * kGrid;
            DrawItem d{};
            d.x = world_from_usecode(static_cast<std::int32_t>(x1));
            d.y = world_from_usecode(static_cast<std::int32_t>(y1));
            d.x_left = world_from_usecode(static_cast<std::int32_t>(x0));
            d.y_far = world_from_usecode(static_cast<std::int32_t>(y0));
            d.z = p.z;
            d.z_top = p.z + 40;
            d.left = screen_x(x0, y1) - view_left_ - 2;
            d.right = screen_x(x1, y0) - view_left_ + 2;
            d.top = screen_y(x0, y0, p.z) - view_top_ - static_cast<int>(kFlameHeight) - 2;
            d.bottom = screen_y(x1, y1, p.z) - view_top_ + 2;
            if (d.right < 0 || d.bottom < 0 || d.left > width || d.top > height) continue;
            std::size_t after = list.size();
            for (std::size_t i = list.size(); i-- > 0;) {
                if (draw_before(list[i], d).value_or(false)) {
                    after = i;
                    break;
                }
            }
            if (after == list.size()) fire_first_.emplace_back(pi, k);
            else fire_after_[after].emplace_back(pi, k);
        }
    }
}

// @orig none: port-only barrel leaks
void BarrelLeaks::put(Surface8& screen, int x, int y, Tone tone, bool glows) {
    if (x < 0 || y < 0 || x >= screen.width || y >= screen.height) return;
    std::uint8_t& p = screen.row(y)[x];
    p = tone_[tone][p];
    if (glows) glowing_.emplace_back(static_cast<std::uint32_t>(y) * static_cast<std::uint32_t>(screen.width) + x, p);
}

// @orig none: port-only barrel leaks
bool BarrelLeaks::opaque(const DrawItem& d, int sx, int sy) const {
    const ShapeFrame& f = *d.frame;
    auto [it, made] = masks_.try_emplace(d.frame);
    std::vector<std::uint8_t>& mask = it->second;
    if (made) {
        mask.assign(static_cast<std::size_t>(f.width) * f.height, 0);
        for (int y = 0; y < f.height; ++y)
            for (int x = 0; x < f.width; ++x) mask[static_cast<std::size_t>(y) * f.width + x] = f.pixel(x, y) >= 0;
    }
    const int fy = sy - d.top;
    const int fx = d.mirrored ? d.right - 1 - sx : sx - d.left;
    if (fx < 0 || fy < 0 || fx >= f.width || fy >= f.height) return false;
    return mask[static_cast<std::size_t>(fy) * f.width + fx];
}

// @orig none: port-only barrel leaks
void BarrelLeaks::draw_first(Surface8& screen) {
    if (!tone_ready_) return;
    for (const auto& [pi, k] : fire_first_) draw_patch(screen, pools_[pi], pools_[pi].patches[k]);
}

// @orig none: port-only barrel leaks
void BarrelLeaks::draw(Surface8& screen, const DrawItem& d, std::size_t index) {
    if (!tone_ready_) return;
    const Item& item = *d.item;
    if (is_fuel_barrel(item.shape)) {
        for (const Barrel& b : barrels_)
            if (b.id == item.id && b.shape == item.shape) draw_barrel(screen, d, b);
    } else if (is_npc_id(item.id)) {
        for (const Burning& b : burning_)
            if (b.id == item.id) draw_person(screen, d, b);
    } else if (!pools_.empty() && !item.sprite_only && !(d.shape_flags & ShapeInfo::kTranslucent)) {
        for (const Pool& p : pools_)
            if (std::abs(d.z_top - p.z) <= kHeight) draw_pool(screen, d, p);
    }
    if (const auto found = fire_after_.find(index); found != fire_after_.end()) {
        for (const auto& [pi, k] : found->second) draw_patch(screen, pools_[pi], pools_[pi].patches[k]);
    }
}

// The pool over the part of floor `d` it covers.
// @orig none: port-only barrel leaks
void BarrelLeaks::draw_pool(Surface8& screen, const DrawItem& d, const Pool& p) {
    const Bounds bb = bounds_of(p);
    if (bb.x1 < bb.x0) return;
    // The floor's box, usecode units, against the pool's.
    const double hx0 = d.x_left / 2.0, hx1 = d.x / 2.0, hy0 = d.y_far / 2.0, hy1 = d.y / 2.0;
    if (hx1 < bb.x0 || hx0 > bb.x1 || hy1 < bb.y0 || hy0 > bb.y1) return;
    const double x0 = std::max(hx0, bb.x0), x1 = std::min(hx1, bb.x1);
    const double y0 = std::max(hy0, bb.y0), y1 = std::min(hy1, bb.y1);
    const int left = std::max({d.left, screen_x(x0, y1) - view_left_ - 1, 0});
    const int right = std::min({d.right, screen_x(x1, y0) - view_left_ + 2, screen.width});
    const int top = std::max({d.top, screen_y(x0, y0, p.z) - view_top_ - 1, 0});
    const int bottom = std::min({d.bottom, screen_y(x1, y1, p.z) - view_top_ + 2, screen.height});
    const std::uint32_t tick = now_;
    const double age = p.burning ? static_cast<double>(tick - p.lit_at) : 0;
    for (int py = top; py < bottom; ++py) {
        for (int px = left; px < right; ++px) {
            // The point of the floor's top seen at this pixel.
            const double a = 2.0 * (px + view_left_ + 0.5), b = 4.0 * (py + view_top_ + 0.5 + p.z);
            const double x = (a + b) / 2, y = (b - a) / 2;
            if (x < hx0 || x > hx1 || y < hy0 || y > hy1) continue;
            const double s = field(p, x, y);
            if (s < 1 || !opaque(d, px, py)) continue;
            const double depth = depth_of(s);
            if (p.burnt) {
                put(screen, px, py, noise(x / 4, y / 4, p.seed ^ 7) + depth * 0.4 > 0.75 ? kAshDark : kAsh);
                continue;
            }
            if (p.burning) {
                const double from = std::sqrt((x - p.fire_x) * (x - p.fire_x) + (y - p.fire_y) * (y - p.fire_y));
                if (age * kSpread >= from) {
                    // Glowing under the fire, burnt where it went out.
                    if (fire_at(p, x, y, tick) > 0) put(screen, px, py, kHot, true);
                    else put(screen, px, py, noise(x / 4, y / 4, p.seed ^ 7) > 0.6 ? kAshDark : kAsh);
                    continue;
                }
            }
            Tone tone = depth < 0.08 ? kRim : depth > 0.6 ? kDeep : kBody;
            if (depth > 0.2 && noise(x / 16 + tick * 0.004, y / 9, p.seed ^ 1) > 0.84) tone = kSheen;
            put(screen, px, py, tone);
        }
    }
}

// The holes of a barrel, the fuel running down under them and out of them.
// @orig none: port-only barrel leaks
void BarrelLeaks::draw_barrel(Surface8& screen, const DrawItem& d, const Barrel& b) {
    const Item& item = *d.item;
    const std::uint32_t tick = now_;
    const auto at = [&](double x, double y, double z, int& sx, int& sy) {
        sx = d.sx + screen_x(x, y) - screen_x(item.x, item.y);
        sy = d.sy + screen_y(x, y, z) - screen_y(item.x, item.y, item.z);
    };
    for (const Hole& h : b.holes) {
        if (!h.seen) continue;
        const double hx = b.x + h.x, hy = b.y + h.y, hz = b.z + h.height;
        int sx = 0, sy = 0;
        at(hx, hy, hz, sx, sy);
        if (h.started && h.height < b.height) {
            // Wet down the side to the floor.
            for (int y = sy + 1; y <= sy + static_cast<int>(h.height); ++y)
                if (opaque(d, sx, y)) put(screen, sx, y, kWet);
        }
        const double head = b.level - h.height;
        if (!h.started || head <= 0) continue;
        const double v = kJet * std::sqrt(head);
        if (v >= kDribble) {
            // A jet out of the side, falling to the floor; light flecks run
            // along it.
            int last_x = 1 << 20, last_y = 1 << 20;
            const double land = std::sqrt(2 * (hz - b.floor) / kGravity);
            for (double t = 0; t <= land; t += 0.2) {
                const double z = hz - 0.5 * kGravity * t * t;
                if (z < b.floor) break;
                int jx = 0, jy = 0;
                at(hx + h.ux * v * t, hy + h.uy * v * t, z, jx, jy);
                if (jx == last_x && jy == last_y) continue;
                last_x = jx;
                last_y = jy;
                const int fleck = static_cast<int>(std::floor(t * 1.5 - tick * 0.6));
                const Tone tone = (fleck & 3) == 0 ? kStreamLight : kStream;
                put(screen, jx, jy, tone);
                if (v > 1.6 && t < land * 0.4) put(screen, jx + 1, jy, kStream);
            }
            // A splash where it lands.
            int lx = 0, ly = 0;
            at(hx + h.ux * v * land, hy + h.uy * v * land, b.floor, lx, ly);
            for (int k = 0; k < 3; ++k) {
                const std::uint32_t r = mix(h.seed, static_cast<int>(tick / 3), k);
                put(screen, lx + static_cast<int>(r % 5) - 2, ly - static_cast<int>((r >> 4) % 2), kStreamLight);
            }
        } else {
            // A dribble: drops running down the side, slower the lower the level.
            const std::uint32_t period = 24 + static_cast<std::uint32_t>((kDribble - v) * 80);
            const double t = static_cast<double>((tick + h.seed) % period);
            const double fall = 0.05 * t * t;
            if (fall < h.height) {
                const int y = sy + 1 + static_cast<int>(fall);
                put(screen, sx, y, kStreamLight);
                put(screen, sx, y - 1, kStream);
            }
        }
    }
    // The holes over their jets.
    for (const Hole& h : b.holes) {
        if (!h.seen) continue;
        int sx = 0, sy = 0;
        at(b.x + h.x, b.y + h.y, b.z + h.height, sx, sy);
        if (opaque(d, sx, sy)) put(screen, sx, sy, kHole);
        if (opaque(d, sx + 1, sy)) put(screen, sx + 1, sy, kHole);
    }
}

// Flames rise from each burning row of the column, losing heat as they go
// up (1 / height a pixel), broken up by noise rising faster than them, so
// that tongues lick up and part from the fire; the hotter, the whiter.
// Where fire was already drawn this frame, the hotter stays.
// @orig none: port-only barrel leaks
void BarrelLeaks::fire_column(Surface8& screen, int px, int bottom, const std::vector<float>& base, float height,
                              std::uint32_t seed) {
    if (px < 0 || px >= screen.width || height < 2) return;
    const float t = static_cast<float>(now_);
    const float decay = 1.0f / height;
    const float x = static_cast<float>(px + view_left_);
    float m = 0;
    const int n = static_cast<int>(base.size());
    for (int k = 0;; ++k) {
        const float b = k < n ? base[static_cast<std::size_t>(k)] : 0.0f;
        m = std::max(b, m - decay);
        if (m <= 0) {
            if (k >= n) break;
            continue;
        }
        const int py = bottom - k;
        if (py < 0) break;
        if (py >= screen.height) continue;
        const float y = static_cast<float>(py + view_top_);
        const float sway = 1.3f * std::sin(y * 0.19f + t * 0.08f + x * 0.05f);
        // Streaks taller than wide, rising faster than the flames.
        const float n1 = fast_noise((x + sway) / 3.2f, (y + t * 1.9f) / 11.0f, seed);
        const float n2 = fast_noise(x / 1.7f, (y + t * 3.1f) / 5.0f, seed + 101);
        const float v = m * (0.02f + 1.45f * (0.6f * n1 + 0.4f * n2));
        if (v < 0.1f) continue;
        const Tone tone = v > 1.0f    ? kFlameWhite
                          : v > 0.8f  ? kFlameYellow
                          : v > 0.62f ? kFlameOrange
                          : v > 0.46f ? kFlameDeep
                          : v > 0.32f ? kFlameRed
                          : v > 0.2f  ? kFlameDark
                                      : kFlameSmoke;
        const std::size_t j = static_cast<std::size_t>(py) * static_cast<std::size_t>(screen.width) + px;
        std::uint8_t& pixel = screen.row(py)[px];
        if (j < heat_.size()) {
            if (heat_frame_[j] == frame_ && heat_colour_[j] == pixel && heat_[j] >= v) continue;
            heat_frame_[j] = frame_;
            heat_[j] = v;
        }
        put(screen, px, py, tone, v > 0.32f);
        if (j < heat_.size()) heat_colour_[j] = pixel;
    }
}

// A patch of a burning pool: for each column of its screen, how strongly
// each row of its floor burns, then the flames off them.
// @orig none: port-only barrel leaks
void BarrelLeaks::draw_patch(Surface8& screen, const Pool& p, std::pair<int, int> patch) {
    const float strength = fire_strength(p, now_);
    if (strength <= 0) return;
    const double x0 = p.grid_x + patch.first * kGrid, y0 = p.grid_y + patch.second * kGrid;
    const double x1 = x0 + kPatch * kGrid, y1 = y0 + kPatch * kGrid;
    const int left = screen_x(x0, y1) - view_left_, right = screen_x(x1, y0) - view_left_;
    const int top = screen_y(x0, y0, p.z) - view_top_, bottom = screen_y(x1, y1, p.z) - view_top_;
    const float t = static_cast<float>(now_);
    for (int px = std::max(left, 0); px <= std::min(right, screen.width - 1); ++px) {
        column_.assign(static_cast<std::size_t>(bottom - top + 1), 0.0f);
        bool any = false;
        for (int k = 0; k <= bottom - top; ++k) {
            const int py = bottom - k;
            const double a = 2.0 * (px + view_left_ + 0.5), b = 4.0 * (py + view_top_ + 0.5 + p.z);
            const double x = (a + b) / 2, y = (b - a) / 2;
            if (x < x0 || x >= x1 || y < y0 || y >= y1) continue;
            const float f = static_cast<float>(fire_at(p, x, y, now_));
            column_[static_cast<std::size_t>(k)] = f;
            any = any || f > 0;
        }
        if (!any) continue;
        // Taller here, lower there, slowly changing.
        const float tall = 0.45f + 0.9f * fast_noise((px + view_left_) / 9.0f + t * 0.012f, t * 0.006f, p.seed);
        fire_column(screen, px, bottom, column_, kFlameHeight * strength * tall, p.seed);
    }
}

// Someone on fire: flames over their body, here and there, rising off it;
// weaker once out of the fire, until they go out.
// @orig none: port-only barrel leaks
void BarrelLeaks::draw_person(Surface8& screen, const DrawItem& d, const Burning& b) {
    const Item& item = *d.item;
    if (item.shape == 0x576 || item.shape == 0x58F) return;  // the game's own burning and burnt
    const float left = b.until > now_ ? static_cast<float>(b.until - now_) / kAfterBurn : 0.0f;
    const float strength = 0.35f + 0.65f * std::min(1.0f, left * 1.5f);
    const float t = static_cast<float>(now_);
    const int rows = d.bottom - d.top;
    const std::uint32_t seed = mix(item.id);
    for (int px = std::max(d.left, 0); px < std::min(d.right, screen.width); ++px) {
        column_.assign(static_cast<std::size_t>(rows), 0.0f);
        bool any = false;
        for (int k = 0; k < rows; ++k) {
            const int py = d.bottom - 1 - k;
            if (!opaque(d, px, py)) continue;
            const float patchy = fast_noise((px + view_left_) / 3.0f, (py + view_top_ + t * 1.2f) / 4.0f, seed);
            if (patchy < 0.45f) continue;
            const float low = k < rows * 0.6f ? 0.85f : 0.6f;
            column_[static_cast<std::size_t>(k)] = low * strength;
            any = true;
        }
        if (any) fire_column(screen, px, d.bottom - 1, column_, 16 * strength, seed);
    }
}

// @orig none: port-only barrel leaks
std::vector<PointLight> BarrelLeaks::lights(const World& world) const {
    std::vector<PointLight> out;
    const float t = static_cast<float>(now_);
    for (const Pool& p : pools_) {
        const float strength = fire_strength(p, now_);
        if (strength <= 0) continue;
        double sx = 0, sy = 0, sw = 0, extent = 0;
        for (const Lobe& l : p.lobes) {
            sx += l.x * l.r * l.r;
            sy += l.y * l.r * l.r;
            sw += l.r * l.r;
            extent = std::max(extent, l.r);
        }
        if (sw <= 0) continue;
        const float flicker = 0.85f + 0.1f * std::sin(t * 0.5f + p.serial) + 0.05f * std::sin(t * 1.3f + p.serial * 3);
        PointLight l;
        l.pos = {static_cast<float>(sx / sw * 2), static_cast<float>(sy / sw * 2), (p.z + 22) * kLightZScale};
        const float k = 1.6f * strength * flicker;
        l.color = Vec3{1.0f * k, 0.52f * k, 0.18f * k};
        l.radius = static_cast<float>(420 + 4 * extent);
        out.push_back(l);
    }
    for (const Burning& b : burning_) {
        const Item* item = world.get(b.id);
        if (!item) continue;
        const float flicker = 0.8f + 0.2f * std::sin(t * 0.7f + b.id);
        PointLight l;
        l.pos = {float(world_from_usecode(item->x)) - 16, float(world_from_usecode(item->y)) - 16,
                 (item->z + 24) * kLightZScale};
        l.color = Vec3{1.0f * flicker, 0.5f * flicker, 0.16f * flicker};
        l.radius = 300;
        l.owner = b.id;
        out.push_back(l);
    }
    return out;
}

// @orig none: port-only barrel leaks
std::vector<Vec3> BarrelLeaks::smoke_since(std::uint32_t since) const {
    std::vector<Vec3> out;
    if (now_ - since > 60) since = now_ - 60;
    for (const Pool& p : pools_) {
        if (!p.burning || p.fuel.empty()) continue;
        for (std::uint32_t t = since + 1; t <= now_ && t != 0; ++t) {
            if ((t + p.serial) % 7 != 0 || fire_strength(p, t) < 0.4f) continue;
            // A cell picked at random: smoke if it burns.
            const std::uint32_t h = mix(p.seed, static_cast<int>(t), 5);
            const int i = static_cast<int>(h % static_cast<std::uint32_t>(p.grid_w));
            const int j = static_cast<int>((h >> 12) % static_cast<std::uint32_t>(p.grid_h));
            const double x = p.grid_x + (i + 0.5) * kGrid, y = p.grid_y + (j + 0.5) * kGrid;
            if (fire_at(p, x, y, t) < 0.3) continue;
            out.push_back({static_cast<float>(x * 2), static_cast<float>(y * 2), (p.z + 20) * kLightZScale});
        }
    }
    return out;
}

// @orig none: port-only barrel leaks
void BarrelLeaks::clear() {
    barrels_.clear();
    pools_.clear();
    burning_.clear();
    burns_.clear();
    fire_after_.clear();
    fire_first_.clear();
    glowing_.clear();
    masks_.clear();
    started_ = false;
}

}  // namespace crusader
