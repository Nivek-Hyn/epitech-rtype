# R-Type

A C++ multiplayer shoot'em up built with our own reusable game engine. Up to four players connect to an
authoritative UDP server. The current prototype includes movement, shooting, enemies and missile collisions.

Linux and native Windows builds and two-player cross-play have been tested. See the
[validation record](docs/validation.md) for exact revisions and outstanding checks. The final campaign,
four-player acceptance and accessibility features remain tracked work.

## Requirements

- A C++20 compiler; tested with GCC 15 and MSVC 2022. See the validation record for exact versions.
- CMake 3.28+ for the client, or 3.24+ for a server-only build, and the selected generator's build tool.
- Git and Internet access for the first dependency download. CMake/CPM fetches Asio, SFML and GoogleTest.
- On Linux, SFML also needs platform development libraries:
- A C++20 compiler: GCC 12+, Clang 16+ or MSVC 2022
- CMake 3.24+
- Git (dependencies are downloaded automatically by [CPM](https://github.com/cpm-cmake/CPM.cmake))
- Linux is required. Windows is a project target, but has not yet been validated for this branch. See the
  [cross-platform guide](docs/cross-platform.md).
- On Linux, install the system packages needed by SFML:
  - Fedora: `sudo dnf install libX11-devel libXrandr-devel libXcursor-devel libXi-devel mesa-libGL-devel systemd-devel`
  - Ubuntu/Debian: `sudo apt install libx11-dev libxrandr-dev libxcursor-dev libxi-dev libgl1-mesa-dev libudev-dev`

These platform libraries and graphics drivers are separate from application dependencies managed by CPM.

## Build and run

From a clone of this repository, on Linux:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
./r-type_server 4242
# In another terminal, from the same repository directory:
./r-type_client 127.0.0.1 4242
```

For Windows build commands and current support status, see the [cross-platform guide](docs/cross-platform.md).

## Run

```powershell
cmake -S . -B build/windows -G "Visual Studio 17 2022" -A x64
cmake --build build/windows --config Release --parallel 4
.\Release\r-type_server.exe 4242
# In another terminal, from the same repository directory:
.\Release\r-type_client.exe 127.0.0.1 4242
```

Run the client from the repository directory so it finds `assets/`. Default host/port are `127.0.0.1:4242`.
Use arrows or ZQSD to move and Space to shoot. Close the client window to quit; use Ctrl+C in the server terminal.
For another computer, replace the host with the server's LAN address and allow its UDP port on the private network.

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
The separate playable second game remains its own delivery task; it is not part of this prototype's launch commands.

## Project and documentation

| Directory | Responsibility |
|---|---|
| `engine/` | Generic ECS, timing, events, physics, binary buffers and queues; standard library only. |
| `common/` | R-Type protocol, components and constants shared between client and server. |
| `server/` | Authoritative simulation and Asio transport. |
| `client/` | SFML rendering, input and Asio transport. |
| `tests/` | GoogleTest tests. |
| `docs/` | [Documentation index](docs/index.md), [architecture](docs/architecture.md), [study](docs/technical-study.md) and [protocol](docs/protocol.md). |

See [CONTRIBUTING.md](CONTRIBUTING.md) for the workflow and [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt)
for dependency notices and asset provenance. Sprites were supplied by Epitech for this student project.
No project-wide source license or public asset redistribution permission is declared here.

## Team

- [Nivek-Hyn](https://github.com/Nivek-Hyn): engine and project tooling.
- [nael481](https://github.com/nael481): networking.
- [Julien-Lnr](https://github.com/Julien-Lnr) / [Julienl01](https://github.com/Julienl01): client and rendering.
- [katiaP18](https://github.com/katiaP18): gameplay and level content.
