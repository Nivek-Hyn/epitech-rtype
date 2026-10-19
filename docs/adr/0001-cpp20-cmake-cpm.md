# 0001 — C++20, CMake and CPM for dependencies

**Status:** accepted

## Context
The subject requires C++, a build system generator and a package manager. The project must build
on a fresh machine without installing libraries system-wide.

## Options
| Package manager | Pros | Cons |
|---|---|---|
| **CPM** | A single CMake file, nothing to install, works the same in CI | Builds dependencies from source (first build is slower) |
| vcpkg | Large catalog, binary caching | Must be installed and bootstrapped on every machine and in CI |
| Conan | Prebuilt binaries, powerful | Needs Python and profile configuration, more friction |

## Decision
- **C++20**: `std::span`, `std::bit_cast`, aggregate initialization with parentheses; supported by GCC, Clang and MSVC.
- **CMake** as build system generator.
- **CPM** as package manager. Downloads are shared between build folders through `CPM_SOURCE_CACHE`.

## Consequences
- `cmake -S . -B build && cmake --build build` is the only thing a developer needs.
- The CI caches `CPM_SOURCE_CACHE` to avoid downloading dependencies on every run.
