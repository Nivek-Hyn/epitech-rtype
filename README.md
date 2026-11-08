# R-Type

A multiplayer shoot'em up in C++20, built on our own reusable game engine. Up to four players join an
authoritative UDP server, alone or together, on Linux and Windows.

- **Game**: a campaign of 9 levels in 3 worlds with bosses, an infinite mode with random spawns, three difficulties,
  weapons and bonuses, a waiting room, lives and respawns. All content is data: entities are prefabs described by
  their components in `assets/config/entities.json`, and levels are JSON files.
- **Network**: binary protocol over UDP ([specification](docs/protocol.md)), reliable messages with
  acknowledgements, split snapshots, 16-bit positions, 30 snapshots per second with client interpolation,
  rate limits, live statistics (F3).
- **Engine**: ECS with prefabs and entity generations, fixed timestep, event bus, AABB physics, network building
  blocks. It builds and installs on its own, and a second game (`brickrush`) uses it without any R-Type code.
- **Accessibility**: remappable keys, auto fire, high contrast, shapes and letters besides colors.

## Requirements

- A C++20 compiler (tested: GCC 15 on Linux, MSVC 2022 on Windows), CMake 3.28+ (3.24+ for a server-only build).
- Git and Internet access for the first build: [CPM](https://github.com/cpm-cmake/CPM.cmake) downloads Asio,
  SFML, nlohmann/json and GoogleTest.
- On Linux, the system libraries SFML needs:
  - Fedora: `sudo dnf install libX11-devel libXrandr-devel libXcursor-devel libXi-devel mesa-libGL-devel systemd-devel`
  - Ubuntu/Debian: `sudo apt install libx11-dev libxrandr-dev libxcursor-dev libxi-dev libgl1-mesa-dev libudev-dev`

## Build and play

Linux:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
./r-type_client            # menu: SOLO starts a local game, JOIN connects to a server
```

Windows (Developer PowerShell for Visual Studio 2022):

```powershell
cmake -S . -B build/windows -G "Visual Studio 17 2022" -A x64
cmake --build build/windows --config Release --parallel 4
.\Release\r-type_client.exe
```

The menu also opens the campaign map and the level editor (CREATE A LEVEL). A dedicated server for a LAN game: `./r-type_server 4242`, then `./r-type_client <server address> 4242` on each
player's computer (allow the UDP port in the firewall). `./r-type_server --help` lists its options (mode,
difficulty, first level, statistics). Run the programs from the repository or installed directory, where `assets/`
is. Arrows or ZQSD move, Space shoots, F1 auto fire, F2 high contrast, F3 network statistics, Escape returns to
the menu. More details: [cross-platform guide](docs/cross-platform.md), [build guide](docs/build-and-run.md).

For a server-only build, add `-DRTYPE_BUILD_CLIENT=OFF`. To omit tests, add `-DRTYPE_BUILD_TESTS=OFF`.
See [build, installation and archive instructions](docs/build-and-run.md) for downloads, output paths and troubleshooting.

## Checks and engine reuse

```sh
ctest --test-dir build --output-on-failure --no-tests=error
scripts/check.sh  # Bash: formatting, build, clang-tidy and tests; tools must be installed first
```

For Visual Studio builds, use `ctest --test-dir build/windows -C Release --output-on-failure --no-tests=error`.
Tool setup and native Windows quality commands are in the [quality guide](docs/static-analysis.md).

The engine can also be built and installed independently:

```sh
cmake -P engine/cmake/CheckPackage.cmake
```

See [engine/README.md](engine/README.md) and the [extension guide](docs/how-to.md).

## Other programs built with the engine

The same build also produces, at the repository root (run them from there):

| Program | What it is | Controls |
|---|---|---|
| `./brickrush` | A second, unrelated game: a brick breaker. It uses the engine only, not `common/` or the R-Type assets. Bricks, paddle and ball are prefabs of `games/brickrush/assets/bricks.json`; each level is a grid of bricks. | Left/Right or Q/D to move, Space to launch, R to restart |

## Project and documentation

| Directory | Responsibility |
|---|---|
| `engine/` | Generic ECS and prefabs, timing, events, physics, binary buffers, reliable delivery helpers; standard library only. |
| `common/` | R-Type protocol, components and constants shared between client and server. |
| `server/` | Authoritative simulation and Asio transport. |
| `client/` | SFML rendering, menus, input and Asio transport; hosts a local server for solo games. |
| `games/brickrush/` | Second game made with the engine only. |
| `tools/level-editor/` | Reading and writing of level files, used by the level editor of the client. |
| `tests/` | GoogleTest tests. |
| `docs/` | [Documentation index](docs/index.md), [architecture](docs/architecture.md), [study](docs/technical-study.md) and [protocol](docs/protocol.md). |

See [CONTRIBUTING.md](CONTRIBUTING.md) for the workflow and [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt)
for dependency notices and asset provenance. Sprites were supplied by Epitech for this student project.
## License

No license has been chosen yet: until the team picks one, the source code is all rights reserved by its authors.
The sprites were supplied by Epitech and must not be redistributed.

## Team

- [Nivek-Hyn](https://github.com/Nivek-Hyn): engine and project tooling.
- [nael481](https://github.com/nael481): networking.
- [Julien-Lnr](https://github.com/Julien-Lnr) / [Julienl01](https://github.com/Julienl01): client and rendering.
- [katiaP18](https://github.com/katiaP18): gameplay and level content.
