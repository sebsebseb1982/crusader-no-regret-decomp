// Changing maps (ITEMCACH.C 04C9 / 05D6 / 13BE, NPC.C 29FA). The item table
// holds one map at a time: leaving a map destroys its items, arriving reads
// them again from FIXED.DAT. NPCs (ids 1..255) are kept, out of the grid
// while their map is not loaded.

#include "engine/world/world.hpp"

namespace crusader {

// @orig 10B0:04C9
void World::unload_map() {
    // Cell by cell, x inner, y outer, each in chain order.
    for (int cy = 0; cy < kGridSize; ++cy) {
        for (int cx = 0; cx < kGridSize; ++cx) {
            for (ObjId id : std::vector<ObjId>(cell(cx, cy).begin(), cell(cx, cy).end())) {
                Item* item = get(id);
                if (!item) continue;
                // Unconditional, as ITEM_10a8_2dad: event 0x10 even outside the fast area.
                leave_fast_area(*item);
                if (is_npc_id(id)) {
                    unlink_from_cell(*item);
                    item->off_map = true;
                    continue;
                }
                // Item_I_destroy also ends the item's processes (KERNEL_11d8_0d0e),
                // the leave event's among them.
                if (hooks_.fail_processes) hooks_.fail_processes(id);
                destroy(id);
            }
        }
    }
}

// @orig 10B0:05D6
void World::load_map(int map) {
    if (map <= 0 || !fixed_) {
        current_map_ = map;
        return;
    }
    auto cachein = [this](ObjId id) {
        // ITEM_10a8_2a9b: usecode event 4, when the class has one.
        if (hooks_.start_event) hooks_.start_event(id, 4);
    };
    ObjId previous = 0;   // last item read: the container of a deeper record
    ObjId container = 0;
    std::uint16_t depth = 0;
    for (const FixedRecord& r : read_fixed_map(*fixed_, static_cast<std::size_t>(map))) {
        if (r.shape == 0) continue;
        Item item;
        item.shape = r.shape;
        item.frame = r.frame;
        item.flags = r.flags;
        item.quality = r.quality;
        item.npc_number = r.npc_number;
        item.map_number = r.map_number;
        // ITEMDATA_10b8_03ef: DAMAGE.FLX byte 5, without the "present" bit test.
        const std::size_t at = std::size_t{r.shape} * 6 + 5;
        item.hit_points = at < damage_.size() ? damage_[at] : 0;
        // The quality of a container is the head of its contents' chain.
        const std::uint8_t fam = family(item);
        if (fam == kFamilyContainer || fam == kFamilyActorContainer) item.quality = 0;

        ObjId id = 0;
        if (!(r.flags & kItemContained)) {
            item.x = r.x;
            item.y = r.y;
            item.z = r.z;
            item.flags &= static_cast<std::uint16_t>(~0x0001);
            id = add(std::move(item)).id;  // at the head of its cell
            depth = 0;
        } else {
            // x is the nesting depth: deeper than before, the previous item
            // is the container; shallower, go up the containers.
            if (depth < r.x) {
                depth = r.x;
                container = previous;
            }
            for (; r.x < depth; --depth) {
                const Item* c = get(container);
                container = c ? c->parent : 0;
            }
            item.z = r.z;
            id = add(std::move(item)).id;
            push(id);
            pop_to_end(container);
            // Not reproduced: an equipped item (0x0200) in an NPC is equipped
            // again (Actor::I_setEquip); FIXED.DAT has no contained items.
            Item& placed = *get(id);
            if (placed.flags & kItemContained) placed.y = r.y;
        }
        const ShapeInfo* info = shape_info(r.shape);
        if (info && info->is(ShapeInfo::kNoisy)) cachein(id);
        previous = id;
    }
    // The map's NPCs go back to their cells, from the last slot down.
    for (ObjId id = static_cast<ObjId>(kNpcSlots - 1); id >= 1; --id) {
        Item* npc = get(id);
        if (!npc || npc->map_number != map) continue;
        npc->off_map = false;
        unlink_from_cell(*npc);
        link_to_cell(*npc);
        cachein(id);
    }
    current_map_ = map;
}

// @orig 10B0:13BE
void World::move_camera_to(std::int32_t x, std::int32_t y, std::int32_t z, int map) {
    // A scripted view takes precedence over the smooth follow's last goal.
    // Following resumes when the controlled target next moves.
    follow_active_ = false;
    if (map == 0) map = current_map_;
    if (map != current_map_) {
        if (current_map_ != 0) unload_map();
        // The original keeps the old area's bounds while loading, so an NPC
        // put back inside them enters at once; here nothing enters before
        // the new area is computed, then all its cells enter.
        fast_area_ = {};
        clear_tracked();
        load_map(map);
    }
    set_camera(x, y, z);
}

// @orig 10F0:29FA
void World::teleport(ObjId npc, std::int32_t x, std::int32_t y, std::int32_t z, int map) {
    Item* item = get(npc);
    if (!item) return;
    // Out of its cell, and out of the fast area when going to another map.
    if (current_map_ != 0 && item->map_number == current_map_ && on_map(*item)) {
        if (map != current_map_) leave_fast_area(*item);
        unlink_from_cell(*item);
        item->off_map = true;
    }
    item->x = x;
    item->y = y;
    item->z = z & 0xFF;
    item->map_number = static_cast<std::uint8_t>(map);
    clear_tracked();  // TRACKER_1128_0066 + 0000
    if (npc == 1 && state.camera_target == 1) move_camera_to(x, y, z, map);
    // Back at the head of its cell when on the current map (push + pop back).
    if (map == current_map_) {
        item = get(npc);
        item->off_map = false;
        push(npc);
        pop();
    }
}

}  // namespace crusader
