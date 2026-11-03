# 0005 — Engine as a standalone library

**Status:** accepted

## Context
The subject asks for decoupled subsystems. The architecture track selected for part 2 requires demonstrating
engine reuse in another game, without carrying over R-Type rules or its network protocol.

## Decision
The code is split into four modules with one-way dependencies:

```
engine  ←  common  ←  server
                   ←  client
```

| Module | Contains | May depend on |
|---|---|---|
| `engine/` | ECS, fixed timestep, event bus, collision shapes, movement system, thread-safe queue, byte serialization | standard library only |
| `common/` | R-Type protocol, shared components, constants | engine |
| `server/` | Authoritative game logic, server networking | common, engine, Asio |
| `client/` | Rendering, input, audio, client networking | common, engine, Asio, SFML |

## Consequences
- `engine/` must never include anything about R-Type: generic code takes the game's types as template parameters.
- `engine/` has its own CMake project and can be built without the other repository directories.
- Installation exports `RTypeEngine::ECS`, `Core`, `Physics`, `Net`, `Systems` and the `Engine` umbrella
  under the `RTypeEngine::` namespace. Consumers use `find_package(RTypeEngine CONFIG REQUIRED)`.
- Modules are optional at build time. Systems depends on ECS; Core carries the platform thread dependency.
- ECS and Core are static libraries; the other modules are header-only. No dynamic plugin loading is implemented.
- Keeping one repository avoids synchronizing engine versions between repositories during development.
  Exported targets provide a separate consumption boundary without requiring that repository split.
- Installed binaries must match the consumer's platform, compiler ABI and configuration. A Linux installation
  cannot be reused as Windows binaries; each platform builds its own package.

## Verification

`cmake -P engine/cmake/CheckPackage.cmake` builds the engine, installs it and runs a separate consumer using
the exported package. See the [extension walkthrough](../how-to.md#run-the-component-and-system-example)
for manual commands, and `engine/README.md` in the source repository for module options.
The consumer proves package integration and generic movement; a second playable game is a separate delivery.
