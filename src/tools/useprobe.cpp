// Uses every item of a saved game, one at a time, as the player would (usecode
// event 1, "use"), and reports what each one did: frame or shape change,
// usecode faults, unbound intrinsics. Lot 6.3 of the roadmap: which items of
// the scenery still lack an engine part.
//
// Usage: useprobe [data_dir] [save] [--shape S] [--frames N] [--hit D [--hits N]]
//
// With --hit, each item is shot instead (Item_I_receiveHit with D damage of
// fire type 2, from the player; --hits N times, 10 frames apart), whether
// or not its class has "use".
//
// Each item gets a fresh load of the save; only items in the fast area whose
// usecode class has a "use" event are tried (one per shape and frame unless
// --shape is given).

#include "engine/world/inventory.hpp"
#include "engine/world/projectile.hpp"
#include "game/game.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

using namespace crusader;

int main(int argc, char** argv) {
    std::filesystem::path data = "data";
    int save = 1;
    int frames = 300;
    int only_shape = -1;
    bool verbose = false;
    int hit = -1;
    int hits = 1;
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--shape") == 0 && i + 1 < argc) only_shape = std::stoi(argv[++i], nullptr, 0);
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) frames = std::stoi(argv[++i]);
        else if (std::strcmp(argv[i], "--hit") == 0 && i + 1 < argc) hit = std::stoi(argv[++i]);
        else if (std::strcmp(argv[i], "--hits") == 0 && i + 1 < argc) hits = std::stoi(argv[++i]);
        else if (std::strcmp(argv[i], "--verbose") == 0) verbose = true;
        else positional.emplace_back(argv[i]);
    }
    if (positional.size() > 0) data = positional[0];
    if (positional.size() > 1) save = std::stoi(positional[1]);

    struct Target { ObjId id; std::uint16_t shape, frame; };
    std::vector<Target> targets;
    {
        Game game(data);
        game.load_save(save);
        const UsecodeFile& uc = game.machine().usecode();
        std::set<std::pair<std::uint16_t, std::uint16_t>> seen;
        game.world().for_each([&](const Item& i) {
            if (i.parent || is_npc_id(i.id) || !(i.flags & kItemFastArea)) return;
            if (only_shape >= 0 && i.shape != only_shape) return;
            if (hit < 0 && (!uc.has_class(i.shape) || i.shape >= uc.class_count() || !uc.event_offset(i.shape, 1))) return;
            if (only_shape < 0 && !seen.insert({i.shape, i.frame}).second) return;
            targets.push_back({i.id, i.shape, i.frame});
        });
    }
    std::printf("save %d: %zu items to use\n", save, targets.size());
    std::map<std::uint16_t, std::uint32_t> all_unbound;
    for (const Target& t : targets) {
        Game game(data);
        game.load_save(save);
        World& w = game.world();
        for (int f = 0; f < 10; ++f) game.update(1'000'000 / 60);
        const auto before_calls = game.machine().unbound_intrinsic_calls();
        const std::size_t before_faults = game.machine().faults().size();
        const auto count = [&w](bool carried) {
            int n = 0;
            w.for_each([&](const Item& i) { n += carried ? i.parent == 1 : 1; });
            return n;
        };
        const int items_before = count(false), carried_before = count(true);
        using Snapshot = std::map<ObjId, std::tuple<std::uint16_t, std::uint16_t, ObjId, std::uint16_t>>;
        const auto snapshot = [&w] {
            Snapshot snap;
            w.for_each([&](const Item& i) { snap[i.id] = {i.shape, i.frame, i.parent, i.quality}; });
            return snap;
        };
        const Snapshot snap_before = verbose ? snapshot() : Snapshot{};
        if (verbose) {
            // The content markers (shape 0x476) near the item.
            const Item* used = w.get(t.id);
            w.for_each([&](const Item& i) {
                if (i.shape != 0x476 || std::abs(i.x - used->x) > 512 || std::abs(i.y - used->y) > 512) return;
                std::printf("      marker %u at %d,%d,%d q %04X npc %02X map %02X\n", i.id, i.x - used->x,
                            i.y - used->y, i.z - used->z, i.quality, i.npc_number, i.map_number);
            });
            std::printf("      item q %04X\n", used->quality);
        }
        if (hit < 0) use_item(w, t.id);
        for (int k = 0; hit >= 0 && k < hits && w.get(t.id); ++k) {
            apply_damage(game.kernel(), w, game.animations(), t.id, 1, 0, hit, 2);
            if (verbose && w.get(t.id)) std::printf("      hit %d: npc %u flags %04X\n", k, w.get(t.id)->npc_number, w.get(t.id)->flags);
            for (int f = 0; f < 10; ++f) game.update(1'000'000 / 60);
        }
        for (int f = 0; f < frames; ++f) game.update(1'000'000 / 60);
        const Item* it = w.get(t.id);
        std::string what = !it ? "gone" : (it->shape != t.shape || it->frame != t.frame)
                                              ? "-> " + std::to_string(it->shape) + ":" + std::to_string(it->frame)
                                              : "unchanged";
        std::string unbound;
        for (const auto& [n, c] : game.machine().unbound_intrinsic_calls()) {
            const auto b = before_calls.find(n);
            if (b != before_calls.end() && b->second == c) continue;
            char buf[16];
            std::snprintf(buf, sizeof buf, " 0x%03X", n);
            unbound += buf;
            ++all_unbound[n];
        }
        std::string faults;
        for (std::size_t k = before_faults; k < game.machine().faults().size(); ++k) faults += " [" + game.machine().faults()[k].message + "]";
        const int items = count(false) - items_before, carried = count(true) - carried_before;
        if (items) what += " items" + std::string(items > 0 ? "+" : "") + std::to_string(items);
        if (carried) what += " carried" + std::string(carried > 0 ? "+" : "") + std::to_string(carried);
        std::printf("%5u %04X:%-3u %-10s %-24s%s%s%s\n", t.id, t.shape, t.frame,
                    game.machine().usecode().class_name(t.shape).c_str(), what.c_str(),
                    unbound.empty() ? "" : " unbound:", unbound.c_str(), faults.c_str());
        if (verbose) {
            const Snapshot after = snapshot();
            for (const auto& [id, v] : snap_before) {
                const auto a = after.find(id);
                if (a == after.end()) std::printf("      - %u %04X:%u in %u\n", id, std::get<0>(v), std::get<1>(v), std::get<2>(v));
                else if (a->second != v)
                    std::printf("      ~ %u %04X:%u in %u q %u -> %04X:%u in %u q %u\n", id, std::get<0>(v), std::get<1>(v),
                                std::get<2>(v), std::get<3>(v), std::get<0>(a->second), std::get<1>(a->second), std::get<2>(a->second), std::get<3>(a->second));
            }
            for (const auto& [id, v] : after)
                if (!snap_before.count(id))
                    std::printf("      + %u %04X:%u in %u\n", id, std::get<0>(v), std::get<1>(v), std::get<2>(v));
        }
    }
    std::printf("unbound intrinsics, by number of items:\n");
    for (const auto& [n, c] : all_unbound) std::printf("  0x%03X  %u\n", n, c);
    return 0;
}
