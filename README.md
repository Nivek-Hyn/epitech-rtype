# R-Type

A networked multiplayer remake of the classic shoot'em up R-Type, built on our own C++ game engine.
Up to 4 players connect to an authoritative server and fight waves of Bydos together.

## Requirements

- A C++20 compiler: GCC 12+, Clang 16+ or MSVC 2022
- CMake 3.24+
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
With `RTYPE_BUILD_CLIENT=OFF`, only the server and launch instructions are installed.
Use a fresh destination when changing the selected programs; installation does not remove old files.

In Windows PowerShell, use an absolute prefix such as `--prefix "$PWD/build/installed-game"`, keep
`--config Debug` consistent with the build, and launch `.\r-type_server.exe` / `.\r-type_client.exe`.
This first installation step does not bundle operating-system libraries or compiler runtime DLLs;
it is not yet a self-contained Linux/Windows distribution. Windows runtime execution remains to be verified.

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
