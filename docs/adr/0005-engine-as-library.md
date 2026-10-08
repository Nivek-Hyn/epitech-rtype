# 0005 — Engine as a standalone library

**Status:** accepted

## Context
The subject asks for decoupled subsystems, and part 2 may require a reusable engine used by a second game.

## Decision
The code is split into four modules with one-way dependencies:

```
engine  ←  common  ←  server
                   ←  client
```

| Module | Contains | May depend on |
|---|---|---|
| `engine/` | ECS, fixed timestep, thread-safe queue, byte serialization | standard library only |
| `common/` | R-Type protocol, shared components, constants | engine |
| `server/` | Authoritative game logic, server networking | common, engine, Asio |
| `client/` | Rendering, input, audio, client networking | common, engine, Asio, SFML |

## Consequences
- `engine/` must never include anything about R-Type. This is checked in code review.
- Extracting the engine into its own repository in part 2 should only require moving the folder.
