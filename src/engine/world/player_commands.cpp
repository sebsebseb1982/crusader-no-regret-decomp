#include "engine/world/player_commands.hpp"

#include "core/dtable.hpp"
#include "core/language.hpp"
#include "engine/audio/audio.hpp"
#include "engine/world/actor_anim.hpp"
#include "engine/world/inventory.hpp"
#include "engine/world/item_selection.hpp"

namespace crusader {

namespace {

constexpr ObjId kPlayer = 1;

// The last item slot going up: No Regret wraps after 0x2C (1008:0EB5), the
// last one; No Remorse wraps after 0x18, one short of its last slot 0x19.
int last_item_slot_up(const World& world) { return world.remorse() ? 24 : item_slots(world).last; }

constexpr std::uint16_t kMedikit = 0x351;
constexpr std::uint16_t kEnergyCube = 0x582;
constexpr std::uint16_t kDetpac = 0x55F;
constexpr std::uint16_t kDatalink = 0x4D4;
constexpr std::uint16_t kNotUsable = 0x4ED;
constexpr std::uint16_t kKeptWeapon = 0x32E;
// No Regret's: the weapon kept below difficulty 2, the credits, and the items
// of R, V and X; B also sets off 0x63E.
constexpr std::uint16_t kRegretKeptWeapon = 0x334;
constexpr std::uint16_t kCredits = 0x112;
constexpr std::uint16_t kItemR = 0x191;
constexpr std::uint16_t kItemV = 0x4ED;
constexpr std::uint16_t kItemX = 0x61C;
constexpr std::uint16_t kDetpac2 = 0x63E;

constexpr std::uint16_t kActionHolster = 0xB;
constexpr std::uint16_t kActionRunningJump = 0x38;
constexpr std::uint16_t kActionJump = 0x3A;

// From the slot after `current` (step +1 or -1), the first slot holding an
// item, wrapping from `last` to `first` going up and from below `first` to
// `last` going down; the current slot is tried last. 0 when none.
// @orig 1008:0C8E 1008:0D71 1008:0E54 1008:0EB5
int find_slot(const World& world, int current, int step, int first, int last) {
    auto wrap = [&](int slot) { return slot > last ? first : slot < first ? last : slot; };
    const int start = wrap(current + step);
    int slot = start;
    do {
        if (world.npc_equip(kPlayer, slot) != 0) return slot;
        slot = wrap(slot + step);
    } while (slot != start);
    return 0;
}

bool kneeling(const World& world) {
    const auto r = world.npc_record(kPlayer);
    return !r.empty() && (r[npc::kFlags13] & 0x04);
}

// NPC_10f0_3b57: no weapon, or a weapon whose record keeps the actions.
bool weapon_keeps_actions(const World& world) {
    const Item* weapon = world.get(world.active_weapon(kPlayer));
    return !weapon || dtable::weapon(weapon->shape).keeps_actions();
}

// CRUSADER_1138_0014: the controlled NPC's last action is an idle one.
// @orig 1148:004C
bool idle(const World& world) {
    switch (world.npc_word(world.state.controlled_npc, npc::kLastAnim)) {
    case 0: case 1: case 0x0B: case 0x10: case 0x20: case 0x37: case 0x3A: case 0x3B:
        return true;
    default:
        return false;
    }
}

ObjId active_item(const World& world) {
    const int slot = world.npc_word(kPlayer, npc::kActiveItem);
    return slot == 0 ? 0 : world.npc_equip(kPlayer, slot);
}

// Quality - 1; at 0 the item is destroyed (Item_I_destroy: its looping
// sounds stop) and leaves the player's slots. True when it was destroyed.
// @orig 13E8:14B4 (No Remorse)
bool use_up(World& world, ObjId id) {
    const std::uint16_t quality = world.get(id)->quality;
    set_quality_and_combine(world, id, static_cast<std::uint16_t>(quality - 1));
    if (world.get(id)->quality != 0) return false;
    if (Audio* audio = world.audio()) audio->sources.stop_item_loops(id);
    world.destroy(id);
    world.unequip(kPlayer, id);
    return true;
}

}  // namespace

// No Regret (1008:1565) also refuses a frozen player (shape 0x5D6).
bool player_may_act(const World& world) {
    if (world.state.avatar_in_stasis || world.is_dead(kPlayer) || world.state.controlled_npc != kPlayer) return false;
    const Item* player = world.get(kPlayer);
    return world.remorse() || !player || player->shape != 0x5D6;
}

namespace {

// No Regret's use of one of an item (1008:1565): quality - 1 first; at 0 the
// item is used, destroyed and leaves its slot, the next item becoming active
// (before the use when it was the active one, for M, E and X; after it for
// U); otherwise it is only used.
void use_one_regret(World& world, ObjId id, bool active) {
    set_quality_and_combine(world, id, static_cast<std::uint16_t>(world.get(id)->quality - 1));
    if (world.get(id)->quality != 0) {
        use_item(world, id);
        return;
    }
    if (!active && active_item(world) == id) next_item(world);
    use_item(world, id);
    if (Audio* audio = world.audio()) audio->sources.stop_item_loops(id);
    world.destroy(id);
    world.unequip(kPlayer, id);
    if (active) next_item(world);
}

// R, V and Ctrl+D in No Regret: the first item of the shape carried is used.
void use_carried_once(World& world, std::uint16_t shape) {
    if (const ObjId id = carried(world, shape)) use_item(world, id);
}

}  // namespace

namespace {

// The pose of a newly chosen weapon (1148:40DC), none when the player stands
// idle; not while an animation runs.
void pose_for_weapon(Kernel& kernel, World& world, const AnimDat& anims) {
    std::uint16_t pose = 0x40;
    if (kneeling(world)) pose = weapon_keeps_actions(world) ? 0x2E : 0x2F;
    else if (!idle(world)) pose = weapon_keeps_actions(world) ? 4 : 0x25;
    if (pose != 0x40 && !actor_busy(kernel, kPlayer)) set_pose(world, anims, kPlayer, pose);
}

}  // namespace

// No Regret: the previous weapon takes its pose, as the next one does. No
// Remorse (13E8:0C07) only changed the slot.
// @orig 1008:0C8E
void previous_weapon(Kernel& kernel, World& world, const AnimDat& anims) {
    const int current = world.npc_word(kPlayer, npc::kWeapon);
    if (current == 0) return;
    const int slot = find_slot(world, current, -1, weapon_slots(world).first, weapon_slots(world).last);
    if (slot == 0) return;
    world.set_npc_word(kPlayer, npc::kWeapon, static_cast<std::uint16_t>(slot));
    if (!world.remorse()) pose_for_weapon(kernel, world, anims);
}

// No Regret no longer asks that the player be the controlled NPC (No
// Remorse 13E8:0C72 tested DS:2852).
// @orig 1008:0D71
void next_weapon(Kernel& kernel, World& world, const AnimDat& anims) {
    const int current = world.npc_word(kPlayer, npc::kWeapon);
    if (current == 0 || (world.remorse() && world.state.controlled_npc != kPlayer)) return;
    const int slot = find_slot(world, current, +1, weapon_slots(world).first, weapon_slots(world).last);
    if (slot == 0) return;
    world.set_npc_word(kPlayer, npc::kWeapon, static_cast<std::uint16_t>(slot));
    pose_for_weapon(kernel, world, anims);
}

// @orig 1008:0B7F
bool remove_item_cru(Kernel& kernel, World& world, const AnimDat& anims, ObjId item_id) {
    const Item* item = world.get(item_id);
    if (item_id == 0 || !item || carried(world, item->shape) == 0) return false;
    // NPC 10F8:3039: the slot holding the item, 45 (one past the slots) when
    // none does. The original then goes on with "slot 45", which is the
    // energy word that follows the slots in the record: it is cleared below
    // as there.
    std::size_t slot = 0;
    while (slot < static_cast<std::size_t>(npc::kEquipSlots) && world.npc_word(kPlayer, npc::kEquip + 2 * slot) != item_id) ++slot;
    if (slot == 0) return false;
    const auto destroy = [&] {
        if (Audio* audio = world.audio()) audio->sources.stop_item_loops(item_id);
        world.destroy(item_id);
        world.set_npc_word(kPlayer, npc::kEquip + 2 * slot, 0);  // NPC 10F8:2FCE
    };
    if (world.family_of(item->shape) == 10) {
        next_weapon(kernel, world, anims);
        destroy();
        return true;
    }
    set_quality_and_combine(world, item_id, static_cast<std::uint16_t>(item->quality - 1));
    if (world.get(item_id)->quality == 0) destroy();
    return true;
}

// @orig 1008:0E54
void previous_item(World& world) {
    const int current = world.npc_word(kPlayer, npc::kActiveItem);
    if (current == 0) return;
    if (const int slot = find_slot(world, current, -1, item_slots(world).first, item_slots(world).last))
        world.set_npc_word(kPlayer, npc::kActiveItem, static_cast<std::uint16_t>(slot));
}

// @orig 1008:0EB5
void next_item(World& world) {
    const int current = world.npc_word(kPlayer, npc::kActiveItem);
    if (current == 0) return;
    if (const int slot = find_slot(world, current, +1, item_slots(world).first, last_item_slot_up(world)))
        world.set_npc_word(kPlayer, npc::kActiveItem, static_cast<std::uint16_t>(slot));
}

// No Regret: nothing for the credits; 0x4ED and 0x191 are only used, as the
// datalink and the scanner and shields are; any other item is used after
// its quality goes down.
// @orig 1008:1565
void use_active_item(World& world) {
    const ObjId id = active_item(world);
    if (id == 0) return;
    const std::uint16_t shape = world.get(id)->shape;
    if (!world.remorse()) {
        if (shape == kCredits) return;
        if (shape == kDatalink || (shape >= 0x52D && shape <= 0x530) || shape == kItemV || shape == kItemR) {
            use_item(world, id);
            return;
        }
        use_one_regret(world, id, true);
        return;
    }
    if (shape == kNotUsable) return;
    use_item(world, id);
    if (shape == kDatalink || (shape >= 0x52D && shape <= 0x530)) return;
    if (use_up(world, id)) next_item(world);
}

// @orig 1008:1565
void use_carried(World& world, std::uint16_t shape) {
    if (const ObjId id = carried(world, shape)) {
        if (!world.remorse()) {
            use_one_regret(world, id, false);
            return;
        }
        use_item(world, id);
        use_up(world, id);
    }
}

// No Regret sets off the items of shape 0x63E after the detpacs, with a
// second search of the same square ("@ %0x63E = $").
// @orig 1008:1565
void set_off_detpacs(World& world) {
    const Item* player = world.get(kPlayer);
    if (!player) return;
    // 16-bit bounds: one past 0x7FFF becomes negative, then 0x7FFF.
    auto bound = [](std::int32_t v) { return static_cast<std::int32_t>(static_cast<std::int16_t>(v)); };
    for (const std::uint16_t shape : {kDetpac, kDetpac2}) {
        if (shape == kDetpac2 && world.remorse()) break;
        for (ObjId id : world.rect_search(bound(player->x - 0x800), bound(player->y - 0x800),
                                          bound(player->x + 0x800), bound(player->y + 0x800))) {
            const Item* item = world.get(id);
            if (item && item->shape == shape) use_item(world, id);
        }
    }
}

// @orig 1008:1565 10B0:00C1
void drop_weapon(Kernel& kernel, World& world, const AnimDat& anims) {
    const ObjId id = world.active_weapon(kPlayer);
    if (id == 0) return;
    const std::uint16_t kept = world.remorse() ? kKeptWeapon : kRegretKeptWeapon;
    if (world.get(id)->shape == kept && world.state.difficulty < 2) return;
    next_weapon(kernel, world, anims);
    const Item* player = world.get(kPlayer);
    world.push(id);
    world.pop_to_coords(player->x, player->y, player->z);
}

// @orig 1148:2AB9 (Tab and J case near 1148:2C99)
// TODO(regret): 1148:2AB9 (No Remorse 1138:220A) is not in the Ghidra function list: create it, then read
//   the jump (it changed: 0x38 when running, an action 7 appears, 0xB then 0x3A is gone).
void jump(Kernel& kernel, World& world, const AnimDat& anims) {
    if (world.state.controlled_npc != kPlayer) return;
    const std::uint16_t last = world.npc_word(kPlayer, npc::kLastAnim);  // Actor_I_getLastAnimSet
    const bool running = last == 3 || last == 0x30 || last == 0x31 || last == kActionRunningJump;  // 10F0:21A0
    if (!actor_busy(kernel, kPlayer)) {
        if (idle(world)) {
            do_anim(kernel, world, anims, kPlayer, kActionJump, kDirCurrent, 0);
        } else if (running) {
            do_anim(kernel, world, anims, kPlayer, kActionRunningJump, kDirCurrent, 0);
        } else if (do_anim(kernel, world, anims, kPlayer, kActionHolster, kDirCurrent, 0) != 0) {
            do_anim(kernel, world, anims, kPlayer, kActionJump, kDirCurrent, 1);
        }
        return;
    }
    // Busy: a run alone becomes a running jump (KERNEL_11d8_151d, vfunc +0x0C).
    if (kernel.count_processes(kPlayer, ActorAnimProcess::kType) != 1) return;
    ActorAnimProcess* anim = current_anim(kernel, kPlayer);
    if (!anim || !running || anim->action() == kActionRunningJump) return;
    anim->fail();
    do_anim(kernel, world, anims, kPlayer, kActionRunningJump, kDirCurrent, 0);
}

// No Regret (1008:1565, case by case): R uses the item 0x191 carried, V the
// item 0x4ED, X uses one of the items 0x61C as M and E do; Ctrl+D uses the
// datalink and Ctrl+L (0x426) drops the weapon. The toggle of target
// designation and the keys '?' and T are gone. Not reproduced: the status bar
// flash (SPMGR_11c8_0dc1(2)) after M, E, R, U, V, X and Ctrl+D.
// @orig 1008:1565
bool player_command(Kernel& kernel, World& world, const AnimDat& anims, std::uint16_t code) {
    const bool may_act = player_may_act(world);
    if (world.state.selection.active) {
        // 13E8:1214: with G under way, the keys are ignored while the player may act.
        if (!(world.state.selection.pick_all && may_act)) selection_key(world, code);
        return true;
    }
    switch (code) {
    case 'G': case 'g':
    case 'S': case 's':
        if (may_act && !world.state.cru_stasis) {
            world.state.crusader_teleporting = false;  // Ultima8Engine_I_clrCrusaderTeleporting
            start_selection(world, code == 'G' || code == 'g');
        }
        return true;
    case 'Q': case 'q':
        if (may_act) previous_weapon(kernel, world, anims);
        return true;
    case 'W': case 'w':
        if (may_act) next_weapon(kernel, world, anims);
        return true;
    case 'I': case 'i':
        if (may_act) previous_item(world);
        return true;
    case 'O': case 'o':
        if (may_act) next_item(world);
        return true;
    case 'U': case 'u':
        if (may_act) use_active_item(world);
        return true;
    case 'M': case 'm':
        if (may_act && !world.state.cru_stasis) use_carried(world, kMedikit);
        return true;
    case 'E': case 'e':
        if (may_act && !world.state.cru_stasis) use_carried(world, kEnergyCube);
        return true;
    case 'B': case 'b':
        if (may_act) set_off_detpacs(world);
        return true;
    case 'X': case 'x':
        if (world.remorse()) return false;
        if (may_act && !world.state.cru_stasis) use_carried(world, kItemX);
        return true;
    case 'V': case 'v':
        if (world.remorse()) return false;
        if (may_act && !world.state.cru_stasis) use_carried_once(world, kItemV);
        return true;
    case 0x426:  // Ctrl+L
        if (world.remorse()) return false;
        if (may_act) drop_weapon(kernel, world, anims);
        return true;
    case 'R': case 'r': {
        if (!world.remorse()) {
            if (may_act && !world.state.cru_stasis) use_carried_once(world, kItemR);
            return true;
        }
        // No condition: target designation on or off, and the message box.
        bool& on = world.state.target_designation;
        on = !on;
        if (world.hooks().show_message)
            world.hooks().show_message(on ? tr("DESIGNATION DE CIBLE ACTIVE.", "TARGETING RETICLE ACTIVE.")
                                          : tr("DESIGNATION DE CIBLE INACTIVE.", "TARGETING RETICLE INACTIVE."));
        return true;
    }
    case 0x420:  // Ctrl+D
        if (!world.remorse()) {
            if (may_act && !world.state.cru_stasis) use_carried_once(world, kDatalink);
            return true;
        }
        if (world.state.controlled_npc == kPlayer && may_act) drop_weapon(kernel, world, anims);
        return true;
    default:
        return false;
    }
}

}  // namespace crusader
