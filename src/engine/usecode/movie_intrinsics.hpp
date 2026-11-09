#pragma once
// The cutscene intrinsics (ITEM.C 10A8:3A5B-3AF4), numbered as in the French
// CRUSADER.EXE. Arguments: item, name (string), x, y (see movie_player.hpp):
//   0x0D2, 0x00F  MovieGump::I_playMovieCutscene      fade to grey first
//   0x093, 0x0AA  MovieGump::I_playMovieCutsceneAlt   no fade
//   0x060         MovieGump::I_playMovieOverlay
// In the original the movie plays inside the call, which returns 0. Here the
// call hands the name to `play` and ends the kernel pass: the calling
// process goes on after the movie.

#include "engine/ui/movie_player.hpp"
#include "engine/usecode/uc_machine.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace crusader {

using PlayMovie = std::function<void(const std::string& name, MovieKind kind)>;

std::vector<std::uint16_t> register_movie_intrinsics(UCMachine& machine, PlayMovie play);

}  // namespace crusader
