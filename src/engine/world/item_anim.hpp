#pragma once
// Animated scenery: flames, steam, fans, screens... (DLIST.C, segment 1198).
// See docs/animation.md, "Objets animés du décor".
//
// TYPEFLAG byte 4 bits 4-7 give an animation type, byte 5 bits 0-3 a datum
// and bits 4-7 a speed. The game process (type 0x245, 1038:01B1) counts its
// passes (+0x40, copied to +0x44) and asks for an animation pass (DS:2C52)
// unless the game is paused; the next drawing of the view runs it
// (ANIMCACH_1188_1719 -> DLIST_1198_162B). The items animated are those in
// the display list whose type is not 0 and that lie on screen
// (DLIST_1198_1153 / 1398, footpad box against the view), in id order, and
// whose speed is below 2 or whose count / speed changed since the last pass
// (DS:2D3B). Per type, with `n` the shape's frame count and `d` the datum:
//   1, 3  d 0, or d 1 and a 1-in-2 draw: next frame, back to 0 after the
//         last; otherwise the next frame within its block of d frames
//         ((f + 1) % d + f - f % d), a block cut short by the end of the
//         shape going back to its start;
//   2     on a 1-in-2 draw, a random frame;
//   4     frame 0 stays there unless a draw of d + 2 gives 0; other frames
//         go on, back to 0 after the last;
//   5     ITEM_10a8_28E2 (no shape of TYPEFLAG.DAT has it);
//   6     as 1 and 3 but frame 0 is never left (the loop goes back to 1),
//         and with blocks, frames at the start of a block stay.
// Only types 1, 3 and 4 occur in TYPEFLAG.DAT.
//
// Not reproduced: the display list itself (the port tests the frame's box on
// screen rather than the footpad box) and the pass that DS:0636 (animations
// off) and the pause flags (DS:8C3B, [DS:062E] +0x36) skip, which the port
// approximates by not animating while a modal screen is open.

#include "engine/world/world.hpp"

#include <cstdint>
#include <functional>

namespace crusader {

// The number of frames of a shape (SHAPHAND_1078_0187).
using FrameCount = std::function<int(std::uint16_t shape)>;

// One animation pass: `count` is the game process's pass count now, `last`
// its value at the previous pass.
void animate_items(World& world, std::uint32_t count, std::uint32_t last, const FrameCount& frames);

}  // namespace crusader
