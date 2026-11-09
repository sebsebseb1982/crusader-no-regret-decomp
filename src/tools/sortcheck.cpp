// Draw-order stability check: scrolls the camera one pixel at a time over a
// saved game and counts the pairs of overlapping items whose relative order
// changes between two consecutive frames (what shows up as flickering scenery).
//
// Usage: sortcheck [data_dir] N [steps]   state of SAVEGAME/SAVEGAME.N, around the player

#include "core/flex.hpp"
#include "core/typeflags.hpp"
#include "engine/world/render.hpp"
#include "engine/world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <unordered_map>

using namespace crusader;

namespace {

constexpr int kScreenW = 640;
constexpr int kScreenH = 480;

bool rects_overlap(const DrawItem& a, const DrawItem& b) {
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

// Whether an opaque pixel of `a` falls on an opaque pixel of `b`.
bool pixels_overlap(const DrawItem& a, const DrawItem& b) {
    const int x0 = std::max(a.left, b.left), x1 = std::min(a.right, b.right);
    const int y0 = std::max(a.top, b.top), y1 = std::min(a.bottom, b.bottom);
    auto opaque = [](const DrawItem& d, int x, int y) {
        const int fx = d.mirrored ? d.right - 1 - x : x - d.left;
        return d.frame->pixel(fx, y - d.top) >= 0;
    };
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            if (opaque(a, x, y) && opaque(b, x, y)) return true;
        }
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    fs::path data = "data";
    int slot = 1, steps = 64;
    int pos = 0;
    for (int i = 1; i < argc; ++i) {
        if (pos == 0 && fs::is_directory(argv[i])) data = argv[i];
        else if (pos == 0) { slot = std::atoi(argv[i]); pos = 1; }
        else steps = std::atoi(argv[i]);
    }
    try {
        const auto shapes_flex = FlexArchive::open(data / "STATIC/SHAPES.FLX");
        const auto globs = FlexArchive::open(data / "STATIC/GLOB.FLX");
        const auto typeflags = load_typeflags(data / "STATIC/TYPEFLAG.DAT");
        ShapeCache shapes(shapes_flex, ShapeLayout::World);
        const auto save = FlexArchive::open(data / "SAVEGAME" / ("SAVEGAME." + std::to_string(slot)));
        World world(typeflags);
        world.load_saved_items(save);
        world.expand_glob_eggs(globs);
        const Item* player = world.get(1);
        if (!player) {
            std::fprintf(stderr, "no player in save %d\n", slot);
            return 1;
        }
        const int x = world_from_usecode(player->x), y = world_from_usecode(player->y);
        const View base{((x - y) >> 2) - kScreenW / 2, ((x + y) >> 3) - player->z - kScreenH / 2};

        std::size_t changed = 0, compared = 0, cycles = 0, constrained = 0, violated = 0, free_flips = 0;
        std::vector<std::pair<ObjId, ObjId>> samples, violations, free_samples;
        std::vector<DrawItem> prev;
        std::unordered_map<ObjId, std::size_t> prev_order;
        for (int step = 0; step <= steps; ++step) {
            // Scroll diagonally: both axes change which items are clipped.
            const View view{base.left + step, base.top + step / 2};
            RenderStats st;
            const auto list = build_display_list(world, shapes, view, kScreenW, kScreenH, &st);
            cycles += st.cycle_edges;
            std::unordered_map<ObjId, std::size_t> order;
            for (std::size_t i = 0; i < list.size(); ++i) order[list[i].item->id] = i;
            // Pairs that constrain each other and were in the previous frame too.
            for (std::size_t i = 0; i < list.size(); ++i) {
                const auto a = prev_order.find(list[i].item->id);
                const bool in_prev = a != prev_order.end();
                for (std::size_t j = i + 1; j < list.size(); ++j) {
                    if (!rects_overlap(list[i], list[j])) continue;
                    const auto rule = draw_before(list[i], list[j]);
                    if (!rule) {
                        // No constraint: only a problem when opaque pixels meet and the order flips.
                        const auto b = prev_order.find(list[j].item->id);
                        if (in_prev && b != prev_order.end() && a->second > b->second &&
                            pixels_overlap(list[i], list[j])) {
                            ++free_flips;
                            if (free_samples.size() < 10) free_samples.emplace_back(list[i].item->id, list[j].item->id);
                        }
                        continue;
                    }
                    ++constrained;
                    // list[i] is drawn first, so the constraint is violated when it says i goes after j.
                    if (!*rule) {
                        ++violated;
                        if (violations.size() < 6) violations.emplace_back(list[i].item->id, list[j].item->id);
                    }
                    const auto b = prev_order.find(list[j].item->id);
                    if (!in_prev || b == prev_order.end()) continue;
                    ++compared;
                    if (a->second > b->second) {
                        ++changed;
                        if (samples.size() < 8) samples.emplace_back(list[i].item->id, list[j].item->id);
                    }
                }
            }
            prev_order = std::move(order);
            prev = list;
        }
        std::printf("save %d: %zu constrained pairs, %zu drawn against their constraint; %zu pairs seen twice, %zu "
                    "changed order; %zu cycle edges\n",
                    slot, constrained, violated, compared, changed, cycles);
        auto show = [&](const char* what, const std::vector<std::pair<ObjId, ObjId>>& pairs) {
            for (const auto& [a, b] : pairs) {
                const Item* ia = world.get(a);
                const Item* ib = world.get(b);
                if (ia && ib) std::printf("  %s: id %u (shape %u) / id %u (shape %u)\n", what, a, ia->shape, b, ib->shape);
            }
        };
        std::printf("  %zu unconstrained pairs with touching opaque pixels changed order\n", free_flips);
        show("violated", violations);
        show("flicker", samples);
        show("free flip", free_samples);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    return 0;
}
