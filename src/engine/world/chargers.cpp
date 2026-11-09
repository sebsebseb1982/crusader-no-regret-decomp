#include "engine/world/chargers.hpp"

#include "engine/audio/audio.hpp"

#include <algorithm>

namespace crusader {

namespace {

constexpr ObjId kPlayer = 1;
constexpr std::uint16_t kType = 0x254;
constexpr int kPlayerMaxHp = 150;  // NPC_10f0_3375

// One of the two stations: `read` / `write` the value it raises by `step`.
// @orig 1130:1D70 1130:1E7B 1130:1F1A 1130:2025
class Charger : public Process {
public:
    Charger(World& world, int target, int step, int busy_sfx, int tick_sfx, std::size_t offset)
        : world_(world), target_(target), step_(step), busy_sfx_(busy_sfx), tick_sfx_(tick_sfx), offset_(offset) {
        set_type(kType);
        set_owner(kPlayer);
        world_.state.avatar_in_stasis = true;
        if (Audio* audio = world_.audio()) audio->sources.play_ambient(world_, busy_sfx_, kPlayer);
    }

protected:
    void run() override {
        auto r = world_.npc_record(kPlayer);
        if (r.empty()) {
            terminate();
            return;
        }
        const int value = r[offset_] | r[offset_ + 1] << 8;
        if (value == target_) {
            terminate();
            return;
        }
        if (Audio* audio = world_.audio(); audio && !audio->sources.is_playing(tick_sfx_, kPlayer))
            audio->sources.play(world_, tick_sfx_, kPlayer);
        const int next = std::min(target_, value + step_);
        r[offset_] = static_cast<std::uint8_t>(next);
        r[offset_ + 1] = static_cast<std::uint8_t>(next >> 8);
        sleep(1);
    }

    void on_terminate() override {
        if (Audio* audio = world_.audio()) audio->sources.stop(busy_sfx_, kPlayer);
        world_.state.avatar_in_stasis = false;
    }

private:
    World& world_;
    int target_, step_, busy_sfx_, tick_sfx_;
    std::size_t offset_;  // in the NPC record: +0 hit points, +0x57 energy
};

}  // namespace

// @orig 10F0:348B
int max_energy(const World& world, ObjId npc) {
    if (npc != kPlayer) return 0;
    switch (world.state.battery) {
    case 1: return 2500;
    case 2: return 5000;
    case 3: return 10000;
    default: return 0;
    }
}

Pid start_healer(Kernel& kernel, World& world) {
    return kernel.spawn<Charger>(world, kPlayerMaxHp, 1, 0xDB, 0xBA, 0x00).pid();
}

Pid start_battery_charger(Kernel& kernel, World& world) {
    return kernel.spawn<Charger>(world, max_energy(world, kPlayer), 25, 0xA4, 0x10B, 0x57).pid();
}

}  // namespace crusader
