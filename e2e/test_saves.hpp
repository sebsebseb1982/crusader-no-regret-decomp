#pragma once
// The saved games the tests rely on, kept outside this repository (CMake passes
// the folder CRUSADER_TEST_SAVES_DIR as CRUSADER_TEST_SAVES): the original ones (SAVEGAME.1..7 and
// the quick save SAVEGAME.13: SEB..SEB5, MISSION2, MISSION3, copied from the
// DOS install) and copies of the saves of reported bugs, named after the bug.
// They are kept apart from data/SAVEGAME, where the game writes the player's
// saves. The tests that load them also need the game data (data/STATIC).

#include <cstdio>
#include <filesystem>

#ifndef CRUSADER_TEST_SAVES
#define CRUSADER_TEST_SAVES "private/e2e/saves"
#endif

namespace crusader {

inline std::filesystem::path test_saves(const std::filesystem::path& /*data*/) { return CRUSADER_TEST_SAVES; }

// False, with a message, when the test saves are not found.
inline bool have_test_saves(const std::filesystem::path& data) {
    if (std::filesystem::exists(test_saves(data) / "SAVEGAME.1")) return true;
    std::printf("missing %s: the test saves (CRUSADER_TEST_SAVES_DIR)\n", test_saves(data).string().c_str());
    return false;
}

}  // namespace crusader
