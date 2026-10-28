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
Its umbrella CMake target is `rtype_engine`, also available as `RTypeEngine::Engine`. It links the enabled
modules, providing their public headers and the C++20 requirement. Core also provides the platform's
standard thread support.

## Select modules

All modules are enabled by default, preserving the full game's configuration.

| Option | Target | Contents | Dependency |
|---|---|---|---|
| `RTYPE_ENGINE_BUILD_ECS` | `RTypeEngine::ECS` | Entities, components and Registry | Standard library |
| `RTYPE_ENGINE_BUILD_CORE` | `RTypeEngine::Core` | FixedTimestep, EventBus, ThreadSafeQueue | Standard thread support |
| `RTYPE_ENGINE_BUILD_PHYSICS` | `RTypeEngine::Physics` | AABB and overlap detection | Standard library |
| `RTYPE_ENGINE_BUILD_NET` | `RTypeEngine::Net` | ByteWriter and ByteReader, without sockets or Asio | Standard library |
| `RTYPE_ENGINE_BUILD_SYSTEMS` | `RTypeEngine::Systems` | Generic movement system | ECS |

ECS and Core each produce a static library. Physics, Net and Systems are header-only targets, so they do
not produce library files. Engine is an interface target grouping the enabled modules; it no longer
produces a separate `librtype_engine.a` or `rtype_engine.lib`.

For a physics-only installation:

```sh
cmake -S engine -B build/engine-physics -DRTYPE_ENGINE_BUILD_ECS=OFF -DRTYPE_ENGINE_BUILD_CORE=OFF -DRTYPE_ENGINE_BUILD_NET=OFF -DRTYPE_ENGINE_BUILD_SYSTEMS=OFF
cmake --build build/engine-physics --config Debug
cmake --install build/engine-physics --config Debug --prefix "$PWD/build/engine-physics-install"
```

Systems cannot be enabled when ECS is disabled; configuration reports an explicit error.
Only enabled modules' headers and targets are installed. Use a fresh installation directory when changing
the module selection: CMake install does not remove files left by an older installation. In a source build,
the shared include directory still exposes all source headers; these options are not an access restriction.

## Install and use from another project

After building the engine, install it into a local directory; administrator privileges are not needed:

```sh
cmake --install build/engine-standalone --config Debug --prefix "$PWD/build/engine-install"
```

The installation contains the enabled modules' libraries and public headers, plus CMake package files.
A consumer using every installed module only needs:

```cmake
find_package(RTypeEngine 0.1 CONFIG REQUIRED)
target_link_libraries(my_game PRIVATE RTypeEngine::Engine)
```

Consumers can also require and link specific components:

```cmake
find_package(RTypeEngine 0.1 CONFIG REQUIRED COMPONENTS Physics)
target_link_libraries(my_game PRIVATE RTypeEngine::Physics)
```

Component names are case-sensitive. Requesting a missing required component fails during configuration.
The basic example needs ECS and Systems; a physics-only package cannot build it.

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

## Check the installed package

From the repository root, run the same command on Linux or Windows:

```sh
cmake -P engine/cmake/CheckPackage.cmake
```

This builds and installs all engine modules, then builds and runs the separate movement example
against that installation. It checks package consumption and propagation of module dependencies.
It does not build the game or download dependencies. Results stay in
`build/engine-package-check-portable`, including an install path containing a space.
The compiler and build tool must already be available in the terminal; with Visual Studio,
use a Developer PowerShell or Developer Command Prompt.

Optional settings go before `-P`, for example:

```sh
cmake -DENGINE_CHECK_CONFIGURATION=Release -DENGINE_CHECK_ROOT=build/engine-package-check-release -P engine/cmake/CheckPackage.cmake
```

Use `-DENGINE_CHECK_GENERATOR=Ninja` to choose a generator. Use a different check directory when
switching compilers or generators. The command must be run on each target operating system;
a successful Linux run does not validate Windows binaries.

The full game still uses the repository's root CMake project. See the
[engine guide](../docs/engine.md) for the API and its current limitations.
