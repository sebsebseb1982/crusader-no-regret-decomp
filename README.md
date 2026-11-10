# Crusader: No Regret — DECOMP

A C++20/SDL2 reimplementation of the *Crusader: No Regret* (Origin, 1996) engine for Windows and Linux.

**Early work in progress: the game is not playable yet.** The first map loads, draws and runs its scripts; see [Status](#status).

![First map of No Regret drawn by the port](docs/images/screenshot.png)

This repository contains **no game files**: you need your own copy of the original game. The version worked on is:

- The **English version 1.06** (CD) — ISO SHA-256: `8EBA7BB0956D539AA841E5D483A6F39222DD6615BA78478544FA516FD7467EBF`

## How this port is made

*No Regret* runs on the engine of *Crusader: No Remorse*, and this project starts from the [No Remorse port](https://github.com/sebsebseb1982/crusader-no-remorse-decomp). A plain copy would miss everything No Regret changed, so the original executable drives the work:

1. Every routine of `REGRET.EXE` was compared with the routine of the No Remorse `CRUSADER.EXE` the port was written from. Of 2 982 routines, 2 260 are identical, 217 differ by constants only, 290 are modified and 215 are new.
2. Every function of the port names the original routine it rewrites (`// @orig <segment:offset>`, an address in `REGRET.EXE`). A function whose routine is not identical in No Regret carries a `TODO(regret)` line until it has been read against `REGRET.EXE` and corrected.
3. The code inherited from No Remorse is kept as is only where the routine is proven identical.

[docs/REGRET_DIFF.md](docs/REGRET_DIFF.md) lists what changed, module by module, and the work left.

## Status

| Area | State |
|---|---|
| Game data recognised (`REGRET.EXE`, 1.06) | done |
| Usecode: the 350 intrinsics numbered; 336 share a routine with No Remorse | done |
| New game: first map loaded and drawn, scripts running without fault | done |
| Functions to review against `REGRET.EXE` (`TODO(regret)`) | 220 left of 349 |
| Intrinsics that exist only in No Regret | 13 of 14 bound (the Xan movie is left) |
| NPC records (45 equipment slots), weapon, item and fire type tables of `REGRET.EXE` | done |
| Saved games (no saved game of the original to compare with yet) | to do |
| Movies (Xan codec), No Regret menus and screens | to do |
| Music: track list, map music, Christmas track | done |
| Sound driver (`ASYLUM.DLL` is another build) | to check |

Everything the No Remorse port adds (gamepad, native resolution, autosave, level editor…) is in the code and will be brought back as the game becomes playable; [enhancements.md](enhancements.md) describes these features as written for No Remorse.

## Building from source

Requirements: CMake 3.21 or later and a C++20 compiler (Visual Studio 2022, GCC 11+, or Clang 14+).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release     # Windows: -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

If SDL2 is installed on your system, the build uses it (`libsdl2-dev` on Linux). Otherwise, SDL2 is downloaded and built automatically.

Put the game data (the contents of the CD: `REGRET.EXE`, `STATIC/`, `USECODE/`, `SOUND/`, `FLICS/`) in `data/`, then:

```sh
./build/Release/crusader data --new-game       # Linux: ./build/crusader data --new-game
```

### Tests

- `tests/`: no game data needed.
- `e2e/regret_data_tests`: the No Regret data (`-DCRUSADER_DATA_DIR=…`; defaults to `data/`), registered only when it is found.
- The other `e2e/` tests come from the No Remorse port and need the No Remorse data (French 1.08) and saved games: `-DCRUSADER_REMORSE_DATA_DIR=…` (defaults to `data_remorse/`) and `-DCRUSADER_TEST_SAVES_DIR=…`. They are registered as `remorse_*` and guard the code both games share.

## Contribution

Contributions are welcome; see [CONTRIBUTING.md](CONTRIBUTING.md). For substantial changes, please open an issue first to discuss the proposed approach.

## License and rights

The port's code and documentation are released under the [GNU GPL v3](LICENSE) or, at your option, any later version. You may use, study, modify, and redistribute them, provided that every redistributed version, modified or not, remains under the same license and includes its source code.

This license applies only to this repository. *Crusader: No Regret* remains the property of its rights holders. This project does not distribute the game's code or data. You must own a copy of the game to play it.
