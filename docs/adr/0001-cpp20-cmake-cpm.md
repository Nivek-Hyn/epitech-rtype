# 0001 — C++20, CMake and CPM for dependencies

**Status:** accepted

## Context
The subject requires C++, a build system generator and a package manager. Application libraries
should be fetched by the build rather than installed system-wide. The compiler, build tools and
platform SDK/system prerequisites still need to be available.

## Options
| Package manager | Pros | Cons |
|---|---|---|
| **CPM** | Integrated into CMake, no separate package-manager bootstrap | Builds dependencies from source (first build is slower) |
| vcpkg | Large catalog, binary caching | Must be installed and bootstrapped on every machine and in CI |
| Conan | Prebuilt binaries, powerful | Needs Python and profile configuration, more friction |

## Decision
- **C++20**: `std::span`, `std::bit_cast`, aggregate initialization with parentheses; supported by GCC, Clang and MSVC.
- **CMake** as build system generator.
- **CPM** as package manager. Downloads are shared between build folders through `CPM_SOURCE_CACHE`.
- Each direct dependency has a `FindXXX.cmake` module called through `find_package(... MODULE)`:
  Asio 1.38.2, SFML 3.1.0, nlohmann/json 3.11.3 and GoogleTest 1.17.0. The modules retain the declared upstream tags,
  build options and target names. They fetch through CPM instead of selecting arbitrary system versions.
  SFML manages its own font/audio dependencies internally.

## Consequences
- After installing the [documented prerequisites](../build-and-run.md), CMake fetches and builds
  the application libraries. Initial configuration needs access to upstream downloads unless cached.
- Client and test dependencies are requested only when their respective build options are enabled.
- The standalone engine does not load the game's dependency modules.
- The CI caches `CPM_SOURCE_CACHE` to avoid downloading dependencies on every run.
