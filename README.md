# R-Type

A networked multiplayer remake of the classic shoot'em up R-Type, built on our own C++ game engine.
Up to 4 players connect to an authoritative server and fight waves of Bydos together.

## Requirements

- A C++20 compiler: GCC 12+, Clang 16+ or MSVC 2022
- CMake 3.28+ for the client (SFML 3.1); 3.24+ for a server-only build
- Git (dependencies are downloaded automatically by [CPM](https://github.com/cpm-cmake/CPM.cmake))
- **Linux only**, system packages needed by SFML:
  - Fedora: `sudo dnf install libX11-devel libXrandr-devel libXcursor-devel libXi-devel mesa-libGL-devel systemd-devel`
  - Ubuntu/Debian: `sudo apt install libx11-dev libxrandr-dev libxcursor-dev libxi-dev libgl1-mesa-dev libudev-dev`

## Build

```sh
export CPM_SOURCE_CACHE=$HOME/.cache/CPM   # optional: share downloaded dependencies between builds
cmake -S . -B build
cmake --build build -j
```

The binaries `r-type_server` and `r-type_client` are created at the repository root.

Options: `-DRTYPE_BUILD_CLIENT=OFF` (server only), `-DRTYPE_BUILD_TESTS=OFF`.

## Run

```sh
./r-type_server [port]          # default port: 4242
./r-type_client [host] [port]   # default: 127.0.0.1 4242
```

Run the client from the repository root so it finds `assets/`.

Asio, SFML and its high-level dependencies are managed by CMake/CPM. The Linux packages above provide
platform libraries such as X11 and OpenGL; they are not replaced by the dependency manager.

## Install locally

After building, install the game's runtime files into a fresh local directory:

```sh
cmake --install build --config Debug --component GameRuntime --prefix "$PWD/build/installed-game"
cd build/installed-game
./r-type_server
# In another terminal, open the same directory:
./r-type_client 127.0.0.1
```

The installation contains the executables, `assets/` when the client is enabled, and `RUNNING.txt` with
launch instructions. It does not need the source tree or build directory at runtime. The working directory
must be the installation directory because the current client uses relative resource paths.
`GameRuntime` selects only game runtime files, excluding dependency development files.
With `RTYPE_BUILD_CLIENT=OFF`, the client and graphical assets are excluded; server notices remain included.
Use a fresh destination when changing the selected programs; installation does not remove old files.

In Windows PowerShell, use an absolute prefix such as `--prefix "$PWD/build/installed-game"`, keep
`--config Debug` consistent with the build, and launch `.\r-type_server.exe` / `.\r-type_client.exe`.
For MSVC Release builds, installation includes compiler runtime files detected by CMake alongside the
executables. Debug installations are intended for development, not clean-machine runtime validation.
Operating-system libraries and graphics drivers are not bundled. Linux binaries still require compatible
system C/C++ runtimes and platform libraries. Windows runtime execution remains to be verified.

## Create a runtime archive

Build the configuration to package, then run CPack from the repository root:

```sh
cmake --build build --config Debug --parallel 4
cpack --config build/CPackConfig.cmake -C Debug -B build/packages
```

The default format is `.tar.gz` on Linux and `.zip` on Windows. Add `-G ZIP` or `-G TGZ` to select either
format explicitly. CPack packages the selected binaries; it does not compile them. For Release, configure
with `-DCMAKE_BUILD_TYPE=Release` when using a single-configuration generator, build Release, and pass
`-C Release`. Keep separate build directories for Debug and Release.

Archives contain one top-level directory with the same runtime layout as the installation above.
Their names include the version, system, processor, enabled programs (`client-server` or `server-only`),
and build configuration. A `.sha256` checksum accompanies each archive. Only `GameRuntime` is packaged:
dependency headers, static libraries, tests and the source tree are excluded.

Extract the archive, open a terminal inside its top-level directory, and follow `RUNNING.txt`.
Main dependency notices are included under `notices/`; see [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt)
for their scope and the asset attribution information still needed from the team.
The runtime-library and platform limitations above still apply. A Linux test does not validate a clean
Windows machine. For the Windows check, build and package Release, extract the ZIP on another Windows
machine, launch the server and client from the extracted directory, and check cross-play with Linux.

## Tests

```sh
ctest --test-dir build --output-on-failure
scripts/check.sh                 # formatting + build without warnings + tests, as the CI does
```

## Project layout

| Folder | Content |
|---|---|
| `engine/` | Generic game engine (ECS, game loop, events, collisions, serialization). Knows nothing about R-Type. |
| `common/` | Code shared by server and client: network protocol, components, constants. |
| `server/` | Authoritative game server. |
| `client/` | Graphical client (SFML). |
| `tests/` | Unit tests (GoogleTest). |
| `docs/` | Documentation, starting at [docs/index.md](docs/index.md) (also buildable as a website, see that page). |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## Authors

<!-- TODO: team members -->
