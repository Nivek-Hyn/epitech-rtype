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

## Tests

```sh
ctest --test-dir build --output-on-failure
```

## Project layout

| Folder | Content |
|---|---|
| `engine/` | Generic game engine (ECS, timing, serialization). Knows nothing about R-Type. |
| `common/` | Code shared by server and client: network protocol, components, constants. |
| `server/` | Authoritative game server. |
| `client/` | Graphical client (SFML). |
| `tests/` | Unit tests (GoogleTest). |
| `docs/` | Documentation: [protocol](docs/protocol.md), [architecture decisions](docs/adr/README.md). |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## Authors

<!-- TODO: team members -->
