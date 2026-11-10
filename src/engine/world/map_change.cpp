// Changing maps (ITEMCACH.C 04C9 / 05D6 / 13BE, NPC.C 29FA). The item table
// holds one map at a time: leaving a map destroys its items, arriving reads
// them again from FIXED.DAT. NPCs (ids 1..255) are kept, out of the grid
// while their map is not loaded.

#include "engine/world/world.hpp"

#include <optional>

namespace crusader {

// @orig 10B8:04C9 (No Remorse 10B0:04C9)
// No Regret adds a test of DS:0ADC and of the static/fixed.dat flex (DS:1462)
// that leads nowhere (dead code).
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

// No Regret empties the tracked items first (TRACKER_1138_0066 + 0000, which
// No Remorse did when teleporting), and reads the maps from
// static/fixed.dat when found (DS:1462, else the archive of DS:145E): the
// port reads it as FIXED.DAT anyway.
// @orig 10B8:05E3 (No Remorse 10B0:05D6)
void World::load_map(int map) {
    if (!remorse_ && map >= 0) clear_tracked();
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

// @orig 1030:07D3 10B8:17DF 10F8:00AB 10B8:1644 10F8:000C 1030:09FE
void World::restart_with_player() {
    // ITEMDATA_10c0_0330 / 0574: what makes an item again.
    struct Kept {
        std::uint16_t shape;
        std::uint16_t frame;
        std::uint16_t quality;
        std::uint8_t npc_number, map_number;
    };
    auto keep = [](const Item& i) { return Kept{i.shape, i.frame, i.quality, i.npc_number, i.map_number}; };
    const Item* p = get(1);
    if (!p) return;
    const Item player = *p;
    const auto rec = npc_record(1);
    std::vector<std::uint8_t> record(rec.begin(), rec.end());
    std::vector<std::optional<Kept>> equipped(static_cast<std::size_t>(equip_slots()));
    for (int slot = 0; slot < equip_slots(); ++slot) {
        if (const Item* i = get(npc_equip(1, slot))) equipped[static_cast<std::size_t>(slot)] = keep(*i);
    }
    // ITEMFIND_10d0_0def with "@ %0x476 = $": the markers in the player.
    std::vector<Kept> markers;
    auto walk = [&](auto&& self, const Item& container) -> void {
        for (ObjId id : container.contents) {
            const Item* i = get(id);
            if (!i) continue;
            if (i->shape == 0x476) markers.push_back(keep(*i));
            self(self, *i);
        }
    };
    walk(walk, player);

    // The tables, empty.
    items_.clear();
    pending_.clear();
    for (auto& cell : cells_) cell.clear();
    tracked_.fill(0);
    tracked_count_ = 0;
    std::fill(npcs_.begin(), npcs_.end(), std::uint8_t{0});
    init_free_slots();
    fast_area_ = {};
    drawn_reset_ = true;
    follow_active_ = false;
    current_map_ = -1;

    // NPC_10f8_000c, then ITEMDATA_10c0_04df: item 1 where it was, on no map.
    Item back;
    back.id = 1;
    back.x = player.x;
    back.y = player.y;
    back.z = player.z;
    back.shape = player.shape;
    back.frame = player.frame;
    back.npc_number = player.npc_number;
    back.off_map = true;
    add_with_id(std::move(back));
    auto make = [this](const Kept& k) -> ObjId {
        const ObjId id = create(k.shape, k.frame);
        Item* i = get(id);
        if (!i) return 0;
        i->quality = k.quality;
        i->npc_number = k.npc_number;
        i->map_number = k.map_number;
        pop_to_container(1);
        if ((i = get(id))) i->flags |= kItemEquipped;
        return id;
    };
    for (std::size_t slot = 0; slot < equipped.size(); ++slot) {
        const ObjId id = equipped[slot] ? make(*equipped[slot]) : 0;
        record[npc::kEquip + 2 * slot] = static_cast<std::uint8_t>(id);
        record[npc::kEquip + 2 * slot + 1] = static_cast<std::uint8_t>(id >> 8);
    }
    auto r = npc_record(1);
    std::copy(record.begin(), record.end(), r.begin());
    for (const Kept& k : markers) make(k);
}

// @orig 10B8:13E7
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

// No Regret keeps the tracked items (No Remorse emptied the tracker here).
// @orig 10F8:30B8
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
    if (remorse_) clear_tracked();  // TRACKER_1128_0066 + 0000
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
