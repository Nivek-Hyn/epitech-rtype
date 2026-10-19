# Technical and Comparative Study

This document explains why we chose each technology, pattern, algorithm and data structure of the project,
compared to the main alternatives. Each important decision also has a short record in [docs/adr](adr/README.md);
this study gathers them, adds the numbers behind them, and covers storage and security.

Status labels used below:
- **Implemented**: in `main`, covered by unit tests.
- **Designed**: the interface and expected behavior exist (headers, tests, specification), the implementation is in progress.
- **Planned**: not started, candidate for part 2.

## 1. Language and tooling

| Need | Choice | Main alternatives | Why |
|---|---|---|---|
| Language | **C++20** | C++17, Rust, C# | Required by the subject. C++20 brings `std::span` (the protocol decoder reads a span of bytes), `std::bit_cast` (sending a float as its bits) and aggregate initialization with parentheses (`registry.emplace<Position>(ship, x, y)`), and is supported by GCC, Clang and MSVC. |
| Build system | **CMake** | Meson, Premake, Makefiles | Required by the subject, cross-platform, understood by every IDE and by the CI. |
| Dependencies | **CPM** | vcpkg, Conan | One CMake file, nothing to install, identical on every machine and in the CI ([ADR 0001](adr/0001-cpp20-cmake-cpm.md)). Cost: dependencies are built from source the first time. |
| Unit tests | **GoogleTest** | Catch2, doctest | Widely used, integrated with CTest (`gtest_discover_tests`), good failure messages. Catch2 or doctest would also fit; the choice is mostly familiarity. |
| Continuous integration | **GitHub Actions**, run on demand + `scripts/check.sh` locally | GitLab CI, Jenkins | The repository is on GitHub. The workflow builds and tests on Linux and Windows and checks formatting. Our Actions quota is used up, so it only runs on demand, and `scripts/check.sh` runs the same checks on each developer's machine. |

## 2. Libraries

### Rendering, audio, input: SFML 3 ([ADR 0002](adr/0002-sfml3-rendering.md))

| Library | Strengths | Weaknesses |
|---|---|---|
| **SFML 3** | C++ API with RAII, graphics + audio + input in one library | Less low-level control |
| SDL3 | Industry standard, very portable | C API, extra libraries for images, fonts, audio mixing |
| Raylib | Very simple to start | C API with global state, harder to isolate in a decoupled engine |

Only the client uses SFML. The engine, the shared code and the server never include it, so the graphics library
could be replaced without touching the game logic.

### Networking: standalone Asio ([ADR 0003](adr/0003-asio-networking.md))

| Option | Strengths | Weaknesses |
|---|---|---|
| **Asio (standalone)** | Cross-platform, asynchronous I/O, header-only, no Boost | Asynchronous model to learn |
| BSD sockets / Winsock | Full understanding of the system API | Two implementations to maintain (Linux and Windows) |

Asio is hidden behind our own interfaces (`INetworkServer`, `INetworkClient`): game code never includes it.

## 3. Architecture and design patterns

### Module layering ([ADR 0005](adr/0005-engine-as-library.md))

```
server ──→ common ──→ engine
   └───────────────────↗
client ──→ common ──→ engine
   └───────────────────↗
```

Dependencies always go down, never up or sideways: `server` and `client` use both `common` and `engine`, never
each other. `engine/` uses the standard library only and knows nothing about R-Type,
so it can be reused by another game. This is why the engine provides *generic* building blocks
(for example `movement<Position, Velocity>` is a template: the game chooses its own component types).

### Entity-Component-System ([ADR 0006](adr/0006-ecs-sparse-arrays.md)) — Implemented

| Option | Strengths | Weaknesses |
|---|---|---|
| Inheritance (`Entity` → `Ship` → `Player`) | Familiar | Rigid hierarchies, logic and data mixed, "diamond" problems |
| **ECS with sparse arrays** | Simple, fast enough, easy to explain and test | Memory used by empty slots |
| ECS with archetypes | Best cache usage for very large worlds | Much more complex (moving entities between tables) |
| Third-party ECS (EnTT, flecs) | Mature and fast | Building the engine is the goal of the project |

An entity is only an id. Components are plain structs. Systems are functions run in a fixed order.
Adding a feature means adding components and a system, without modifying existing classes.

### Mediator / Observer: the event bus — Implemented

The subject recommends the Mediator pattern to decouple systems. `engine::core::EventBus` lets a system publish
an event (for example a collision) without knowing who reacts to it (damage, score, sound).

| Option | Strengths | Weaknesses |
|---|---|---|
| Systems calling each other directly | No extra code | Every system depends on the others |
| Immediate dispatch (`publish`) | Simple, predictable order | A handler runs while the publishing system is still iterating |
| **Immediate + queued dispatch (`enqueue` / `dispatch`)** | Events are delivered at a chosen point, after the systems have run | Events must be copyable |
| Priorities, answers to messages | Needed by complex engines | Not needed by our game yet |

We kept both modes. The queued mode avoids a real bug: if a damage handler kills an entity while the collision
system is still iterating over components, references to that entity's components would become invalid.
Priorities and answers are **planned** if part 2 needs them.

### Fixed timestep game loop — Implemented

| Option | Strengths | Weaknesses |
|---|---|---|
| Variable step (`dt` = frame time) | Simple | Simulation depends on the machine speed; collisions can be missed on slow frames |
| **Fixed step with accumulator** | Same simulation on every machine; required for a server that is the reference | Needs a catch-up strategy |
| Semi-fixed step | Smooth rendering | More complex, not needed on the server |

`engine::core::FixedTimestep` turns real elapsed time into a number of fixed steps (60 per second).
If the server falls behind, it simulates at most 5 steps per loop and drops the remaining whole steps,
keeping the fraction. Without this limit a slow server could accumulate an endless backlog
("spiral of death"). The trade-off: after a long stall the game runs slightly behind real time.

## 4. Algorithms and data structures

| Structure / algorithm | Where | Complexity | Why this one |
|---|---|---|---|
| `std::vector<std::optional<C>>` (sparse array) | `SparseArray<C>` | access O(1), insertion amortized O(1) | Direct indexing by entity id, contiguous memory, no hashing. Empty slots cost `sizeof(std::optional<C>)`: about 12 bytes for a 2-float position, so 10,000 ids use about 120 KB. |
| Free list (LIFO stack of ids) | `Registry::spawn` / `kill` | O(1) | Reuses killed ids so the arrays stay small. |
| `std::unordered_map<std::type_index, ...>` | component storages, event handlers | O(1) average | One storage per component type, created on first use, found by type at run time. |
| Type erasure (`ISparseArray`, `IHandlerList`) | `Registry`, `EventBus` | — | Lets one container hold storages of different types; `kill` can clear every component of an entity. |
| Filtered iteration | `Registry::each<A, B>` | O(number of ids × number of component types) | Simple and fast enough for a few hundred entities; no index to keep in sync. |
| AABB overlap test | `engine::physics::overlaps` | O(1) per pair | Four comparisons; sprites in a shoot'em up are well approximated by rectangles. |
| Pairwise collision checks | gameplay collision system (designed) | O(n²) | With 200 entities: about 20,000 tests per tick, 1.2 million per second, a negligible cost. A uniform grid or quadtree is **planned** only if entity counts grow. |
| Accumulator with catch-up limit | `FixedTimestep::advance` | O(steps) | See section 3. |
| Thread-safe queue | network thread → game thread (designed) | O(1) | A mutex-protected queue is simple and correct; lock-free queues are faster but much harder to get right. |

### Known limits

- Entity ids are recycled without a *generation* counter: an old id kept by mistake may point to a new entity.
  Mitigation: entities are never sent over the network (the `NetworkId` component, never reused, is sent instead),
  and systems do not keep entity ids between ticks. A generation counter is a **planned** improvement.
- Growing a vector or removing a component can invalidate pointers and references to components.
  Callbacks of `each` must not keep them.

## 5. Networking

| Choice | Alternatives | Why |
|---|---|---|
| **UDP** | TCP | Required by the subject for in-game traffic. A lost packet is not retransmitted, so an old position never delays a new one. |
| **Authoritative server**, clients send inputs only ([ADR 0007](adr/0007-authoritative-snapshots.md)) | Peer-to-peer, client authority | The server decides everything: a modified client cannot move faster or teleport, it can only send key presses. |
| **Full snapshots every tick** | Delta snapshots, events only | Self-healing: a lost snapshot is replaced by the next one 16.7 ms later. No reliability layer needed. |
| **Binary protocol** ([ADR 0008](adr/0008-binary-protocol.md), [specification](protocol.md)) | JSON, text | Compact and fast to parse. One entity takes 14 bytes in binary, about 50 to 60 bytes in JSON. |

Bandwidth estimate, worst case (a full snapshot packet: 10-byte header + 11 bytes + 84 entities × 14 bytes = 1,197 bytes):
60 packets per second ≈ 72 KB/s ≈ 575 kbit/s per client, about 2.3 Mbit/s for 4 clients. This is acceptable on a
local network or a home connection. Delta snapshots and a lower send rate with client interpolation are **planned**
for part 2.

The 1,200-byte maximum keeps every datagram under the usual Internet MTU, so packets are never fragmented by IP.

## 6. Storage

The game has very little data to keep, so we compared options against what we actually need.

| Data | Current storage | Persistence needed? |
|---|---|---|
| Game state (entities, score) | Server memory (the ECS) | No: a game lasts a few minutes; if the server stops, the game ends. |
| Assets (sprites, sounds) | Read-only files in `assets/` | Already persistent, never modified. |
| Settings (key bindings, volume) | None yet | Yes, once reconfigurable keys exist (**planned**). |
| High scores | None yet | Possibly in part 2 (**planned**). |

| Technique | Persistence | Reliability | Constraints |
|---|---|---|---|
| **Memory only** (current) | None | Lost on crash or restart | Fastest, nothing to secure on disk |
| Text file (INI, JSON) | Yes | A crash during writing can corrupt the file; write to a temporary file then rename it | Human-readable, easy to edit and to version |
| Custom binary file | Yes | Same risk; needs a version number for format changes | Compact, but harder to debug |
| Embedded database (SQLite) | Yes | Transactions: a crash never leaves a half-written state | Adds a dependency; useful only for queries such as high score tables |
| Database server (PostgreSQL) | Yes | Highest, with replication | Requires a separate service: far too heavy for this game |

**Decision for part 1:** no persistence. Nothing in the current game must survive a restart, and keeping
everything in memory removes a whole class of corruption and security problems.
**For settings:** a small text file per client, written to a temporary file and renamed, is the simplest reliable option.
**For high scores:** SQLite if they are added, because its transactions guarantee consistency.

## 7. Security

### Threat model

The server listens on a public UDP port. Anyone can send it any datagram. The client trusts only the server
it chose to connect to.

| Threat | Mitigation | Status |
|---|---|---|
| Malformed or malicious packet (crash, buffer overflow) | Strict validation: size between 10 and 1,200 bytes, magic, version, exact payload size, known type and enum values. Decoding returns `std::optional` and never throws; invalid packets are dropped silently. Unit tests cover truncated packets and lying counts. | Designed (tests written) |
| Reading past the end of a buffer | `ByteReader` returns 0 and marks itself as failed instead of reading out of bounds. Fields are read one by one, never with `memcpy` of structs. | Designed |
| Cheating with a modified client | Authoritative server: clients only send input flags. Positions, damage and score are computed on the server. | Implemented in the architecture |
| Too many clients / resource exhaustion | At most 4 players; a client silent for 5 seconds is removed. | Designed |
| Flooding the server with packets | Invalid packets are dropped before any game logic. Rate limiting per address is **planned**. | Partial |
| Spoofed source address (sending inputs as another player) | Not covered: UDP has no authentication. A random session token returned in `ConnectAccepted` and required in every packet is **planned**. | Planned |
| Eavesdropping | Not covered: traffic is not encrypted. The game exchanges no personal data, so the risk is low. | Accepted |

### Vulnerabilities of the technologies

| Technology | Main risk | How we reduce it |
|---|---|---|
| C++ | Memory errors: out-of-bounds access, dangling pointers, use after free | Standard containers (`std::vector`, `std::optional`, `std::span`) instead of raw arrays and `new`; `std::unique_ptr` for ownership; strict warnings (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`); unit tests. |
| UDP | No delivery guarantee, no authentication | Full snapshots tolerate losses; validation of every datagram; session token planned. |
| Third-party libraries (Asio, SFML, GoogleTest) | A vulnerability in a dependency | Exact versions pinned in `cmake/Dependencies.cmake` (`asio-1-38-2`, `3.1.0`, `v1.17.0`): a dependency never changes without an explicit commit. |

### Long-term security monitoring

- **Dependencies:** follow the release notes and GitHub security advisories of Asio, SFML and GoogleTest.
  CPM is not supported by Dependabot, so each version bump is a manual pull request.
- **Checks before every push:** `scripts/check.sh` builds with strict warnings, runs the unit tests and checks the
  formatting; the CI workflow does the same on Linux and Windows when it is run.
- **Planned:** build the tests with AddressSanitizer and UndefinedBehaviorSanitizer in the CI, and fuzz the packet
  decoder (for example with libFuzzer), since it is the only code reachable by an attacker.

## 8. Summary of trade-offs

| Decision | What we gain | What we accept |
|---|---|---|
| Our own ECS with sparse arrays | Simplicity, testability, full understanding | Some memory on empty slots; no generation counter yet |
| Fixed timestep with catch-up limit | Same simulation everywhere; no endless backlog | Game slightly behind real time after a long stall |
| Event bus with immediate and queued modes | Decoupled systems, safe deferred reactions | Events must be copyable; no priorities yet |
| Full snapshots over UDP | Robust to packet loss, simple | Higher bandwidth than deltas |
| No persistence | No corruption or disk security issues | Nothing survives a server restart |
