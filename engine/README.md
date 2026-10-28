# R-Type Engine

A generic C++20 library providing an entity-component system, fixed-step timing, events,
thread-safe queues, binary buffers, AABB collision detection and a movement system.
The engine uses the standard library and does not depend on the R-Type game, SFML or Asio.

## Build the engine alone

Requirements: a C++20 compiler, CMake 3.24 or newer, and the build tool used by the selected generator.
No third-party packages are downloaded when configuring the engine alone.

From the repository root:

```sh
cmake -S engine -B build/engine-standalone -DCMAKE_BUILD_TYPE=Debug
cmake --build build/engine-standalone --config Debug -j 4
```

The `engine/` directory can also be copied into another project without the other repository directories.
Its CMake target is `rtype_engine`, also available as `RTypeEngine::Engine`; linking this target provides
the public headers, the C++20 requirement and the platform's standard thread support.

## Install and use from another project

After building the engine, install it into a local directory; administrator privileges are not needed:

```sh
cmake --install build/engine-standalone --config Debug --prefix "$PWD/build/engine-install"
```

The installation contains the static library, public headers and CMake package files. A consumer only needs:

```cmake
find_package(RTypeEngine 0.1 CONFIG REQUIRED)
target_link_libraries(my_game PRIVATE RTypeEngine::Engine)
```

Pass the installation directory through `CMAKE_PREFIX_PATH` when configuring the consumer.
No engine source or build directory is needed by the consumer. The installation can be moved to another
directory on the same platform without rebuilding it. The binaries must still match the consumer's
platform, compiler ABI and build configuration; moving a Linux build to Windows is not supported.

The bundled example is a separate project using only the installed package:

```sh
cmake -S engine/examples/basic -B build/engine-example -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$PWD/build/engine-install"
cmake --build build/engine-example --config Debug -j 4
ctest --test-dir build/engine-example -C Debug --output-on-failure
```

For Visual Studio generators, keep `--config Debug` and `-C Debug`: the executable is placed in a
configuration-specific directory. Build Release separately and use a separate installation directory
when a Release consumer is needed.
Version checks accept compatible patch updates within the same minor version while the engine is at 0.x.

The full game still uses the repository's root CMake project. See the
[engine guide](../docs/engine.md) for the API and its current limitations.
