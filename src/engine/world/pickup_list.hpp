#pragma once
// The items just picked up, listed at the top left of the screen (DISPLAY.C,
// segment 1190; "PickupAreaGump" in ScummVM). Eight entries of 0x19 bytes
// (DS:8346), cleared when the game starts (DISPLAY_1190_00fe):
//   +0 name (far pointer), +4 icon {frame, shape, 0}, +8 count, +0xA icon
//   shape, +0xC icon frame, +0xE seconds left, +0x10 redraw, +0x11 rectangle.
//
// MainActor::I_addItemCru, when asked to show the item, calls
// DISPLAY_1190_049c with it before destroying it:
// - DISPLAY_1190_070f gives the icon (GUMPS.FLX) and the name: keycard 0x111
//   shape 5 frame 13 + its frame, "PASSE"; credits 0x4ED shape 3 frame 13,
//   "CREDITS"; family 10 shape 3, the weapon's icon and name; family 11 shape
//   5, the ammunition's; family 12 shape 4; family 13 shape 5 (dtable.hpp).
//   Any other item stops the original with a fatal assertion (DISPLAY.C line
//   0x136); addItemCru never passes one;
// - an entry with the same icon counts one more (the credits test compares the
//   icon shape with 0x4ED, never true: credits picked up while their entry is
//   shown count 1 each), and is shown 4 more seconds;
// - else the first free entry: count 1, or for credits their quality (1 when
//   0), 4 seconds. Nothing when all eight are used.
// A process (DISPLAY_1190_0000, any level) counts the seconds: once 60 ticks
// have passed since its last count (DISPLAY_1190_0057), each entry loses one,
// and is freed at 0 (DISPLAY_1190_0414). An entry thus stays 3 to 4 seconds.
//
// Drawing: see engine/ui/pickup_area.hpp.

#include "engine/process.hpp"

#include <array>
#include <cstdint>

namespace crusader {

class World;
struct GameState;

struct PickupEntry {
    const char* name = nullptr;  // nullptr: free
    std::uint16_t icon_shape = 0;
    std::uint16_t icon_frame = 0;
    int count = 0;
    int seconds = 0;
};

class PickupList {
public:
    static constexpr int kEntries = 8;
    static constexpr int kSeconds = 4;

    // DISPLAY_1190_049c.
    void add(const World& world, ObjId item);
    // DISPLAY_1190_0414.
    void count_second();
    void clear() { entries_ = {}; }

    const std::array<PickupEntry, kEntries>& entries() const { return entries_; }

private:
    std::array<PickupEntry, kEntries> entries_{};
};

// DISPLAY_1190_0000 / 0057: counts the seconds of GameState::pickups.
class PickupListProcess : public Process {
public:
    explicit PickupListProcess(GameState& state);

protected:
    void on_added() override;
    void run() override;

private:
    GameState* state_;
    std::uint32_t last_ = 0;  // +0x32
};

}  // namespace crusader
