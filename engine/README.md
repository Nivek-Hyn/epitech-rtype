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
Its CMake target is `rtype_engine`; linking this target provides the public headers and the C++20 requirement.

The full game still uses the repository's root CMake project. See the
[engine guide](../docs/engine.md) for the API and its current limitations.
