# Technical and Comparative Study

This document explains why we chose each technology, pattern, algorithm and data structure of the project,
compared to the main alternatives. Each important decision also has a short record in [docs/adr](adr/README.md).
This study separates implementation choices, theoretical estimates and results actually verified.
See the [validation record](validation.md) for tested revisions and remaining checks.

Status labels used below:
- **Implemented**: present in the described code snapshot. This does not mean every feature branch is merged into `main`.
- **Designed**: the interface and expected behavior exist (headers, tests, specification), the implementation is in progress.
- **Planned**: not started, candidate for part 2.

This revision updates the engine and build choices against the combined local work. Network measurements,
complete campaign behavior, persistence and accessibility results still need contributions from their owners.
The server now loads the first level's wave timing, positions and speed from JSON using nlohmann/json 3.11.3.
Content changes need no recompile for those supported values; new movement behaviors still require code.
Only the initial level is selected, and validation of all configuration bounds remains incomplete.

## 1. Language and tooling

| Need | Choice | Main alternatives | Why |
|---|---|---|---|
| Language | **C++20** | C++17, Rust, C# | C++ is required by the subject; C++20 is our choice. It provides `std::span`, `std::bit_cast` and aggregate initialization with parentheses (`registry.emplace<Position>(ship, x, y)`). Tested compiler versions are recorded separately. |
| Build system | **CMake** | Meson, Premake, Makefiles | Meets the build-system-generator requirement and represents targets, module dependencies and installed packages in one configuration. CMake itself is not the only generator allowed by the subject. |
| Dependencies | **CPM** | vcpkg, Conan | Keeps version declarations in CMake ([ADR 0001](adr/0001-cpp20-cmake-cpm.md)). The first build downloads and compiles application libraries. Compilers, SDKs and documented platform libraries such as X11/OpenGL remain prerequisites; builds are not assumed identical on every machine. |
| Unit tests | **GoogleTest** | Catch2, doctest | Widely used, integrated with CTest (`gtest_discover_tests`), good failure messages. Catch2 or doctest would also fit; the choice is mostly familiarity. |
| Continuous integration | **GitHub Actions**, run on demand + `scripts/check.sh` locally | GitLab CI, Jenkins | The repository is on GitHub. The manual workflow builds and tests on Linux and Windows and checks formatting. Local checks additionally run clang-tidy; they do not use Actions credits. |

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

### Standalone engine and module choices — Implemented

`engine/` has its own CMake project. It can be built and installed without `common/`, `client/`, `server/`,
SFML or Asio. A consumer finds the installed package with `find_package(RTypeEngine CONFIG REQUIRED)`.

| Approach | What it offers | Cost or limit |
|---|---|---|
| One static engine library | A single target to maintain and link | Less choice of modules at configuration time. |
| **Separate modules with explicit dependencies** | Consumers select ECS, Core, Physics, Net or Systems; an umbrella target remains convenient | More CMake targets, options and package rules to maintain. |
| Runtime plugins | Modules could be loaded or replaced while the application runs | Requires a loader, ABI agreement and careful lifetime/unloading rules; not implemented. |

ECS and Core produce static libraries. Physics, Net and Systems are header-only; the Engine umbrella is
an interface target. Systems links ECS, and Core carries the platform thread dependency. These relationships
are exported to consumers, together with include paths and the C++20 requirement.

We keep the engine in the same repository for coordinated development, while its independent project and
installed package provide the reuse boundary. Binaries still need a compatible OS, compiler ABI and build
configuration. Module flags operate at build time; there is no dynamic plugin API.

The separate basic consumer tests movement and a user-defined Lifetime component/system. Linux and Windows
package checks are recorded in [validation.md](validation.md). A second playable game remains a separate task.

### Entity-Component-System ([ADR 0006](adr/0006-ecs-sparse-arrays.md)) — Implemented

| Option | Strengths | Weaknesses |
|---|---|---|
| Inheritance (`Entity` → `Ship` → `Player`) | Familiar | Rigid hierarchies, logic and data mixed, "diamond" problems |
| **ECS with sparse arrays** | Simple, fast enough, easy to explain and test | Memory used by empty slots |
| ECS with archetypes | Groups entities with the same component set for traversal | More complex storage and migration rules; no project benchmark comparing it with our arrays. |
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
Priorities and answers are candidates only if a game needs them. The bus is available in the engine;
the current R-Type collision code collects destructions directly rather than using this bus.

### Fixed timestep game loop — Implemented

| Option | Strengths | Weaknesses |
|---|---|---|
| Variable step (elapsed frame time) | Simple | Large steps can miss collisions and change numerical behavior. |
| **Fixed step with accumulator** | A consistent step duration for the authoritative simulation | Needs a catch-up strategy; does not guarantee identical results across all machines. |
| Semi-fixed step | Splits long frames into smaller simulation steps | Requires managing the remainder and workload; does not provide rendering interpolation by itself. |

`engine::core::FixedTimestep` turns real elapsed time into a number of fixed steps (60 per second).
If the server falls behind, it simulates at most 5 steps per loop and drops the remaining whole steps,
keeping the fraction. Without this limit a slow server could accumulate an endless backlog
("spiral of death"). The trade-off: after a long stall the game runs slightly behind real time.

## 4. Algorithms and data structures

| Structure / algorithm | Where | Complexity | Why this one |
|---|---|---|---|
| `std::vector<std::optional<C>>` (sparse array) | `SparseArray<C>` | O(1) indexed lookup; growth can be linear in the new storage size | Direct indexing by entity id. Slots are contiguous, including empty ones. Slot memory depends on `sizeof(std::optional<C>)` and allocated capacity; it is not a portable fixed byte count. |
| Free list (LIFO stack of ids) | `_freeIds` in Registry | O(1) amortized push/pop | Reuses killed ids and reduces future growth. It does not shrink existing component arrays. |
| Entity destruction | `Registry::kill` | O(number of component storages), plus component destruction costs | Every storage must clear that entity's slot before the id can be reused. |
| `std::unordered_map<std::type_index, ...>` | component storages, event handlers | O(1) average | One storage per component type, created on first use, found by type at run time. |
| Type erasure (`ISparseArray`, `IHandlerList`) | `Registry`, `EventBus` | — | Lets one container hold storages of different types; `kill` can clear every component of an entity. |
| Filtered iteration | `Registry::each<A, B>` | Expected O(allocated ids × requested component types), excluding callback work | No secondary index to maintain. Empty/dead ids still contribute to the scan; no performance benchmark is claimed. |
| AABB overlap test | `engine::physics::overlaps` | O(1) per pair | Four comparisons; sprites in a shoot'em up are well approximated by rectangles. |
| Nested missile/enemy checks | `Game::handleCollisions` | O(missiles × enemies), plus ECS scans | Simple implementation for player-missile/enemy collisions. A grid or quadtree is a candidate if profiling shows this becomes expensive; no negligible-cost claim has been measured. |
| Accumulator with catch-up limit | `FixedTimestep::advance` | O(steps) | See section 3. |
| Thread-safe queue | network thread → game thread | Queue operations are constant-time apart from allocation and mutex waiting | `tryPop` does not wait for a new message, but can wait to acquire its mutex. Lock-free alternatives add implementation constraints; no speed comparison has been measured. |

### Known limits

- Entity ids are recycled without a *generation* counter: an old id kept by mistake may point to a new entity.
  The game sends a separate `NetworkId` over the network. That separates the two id spaces, but does not
  detect stale local ids: callers must manage their lifetime. This is a usage rule, not a check enforced by Registry.
  A generation counter is a possible improvement, not part of the current implementation.
- Growing a vector or removing a component can invalidate pointers and references to components.
  Callbacks of `each` must not keep them.
- Removing a component leaves an empty slot. Storage follows the largest inserted id and retained vector capacity,
  rather than just the number of currently live components.

## 5. Networking

| Choice | Alternatives | Why |
|---|---|---|
| **UDP** | TCP | Required by the subject for in-game traffic. UDP does not retransmit lost packets; ordering and stale-state rejection remain application responsibilities. |
| **Authoritative server**, clients send inputs only ([ADR 0007](adr/0007-authoritative-snapshots.md)) | Peer-to-peer, client authority | The server decides everything: a modified client cannot move faster or teleport, it can only send key presses. |
| **Snapshots every tick** | Delta snapshots, events only | A later received snapshot can replace lost state. The next datagram can also be lost or reordered; this does not make individual events reliable. |
| **Binary protocol** ([ADR 0008](adr/0008-binary-protocol.md), [specification](protocol.md)) | JSON, text | Compact and fast to parse. One entity takes 14 bytes in binary, about 50 to 60 bytes in JSON. |

Bandwidth estimate, worst case (a full snapshot packet: 10-byte header + 11 bytes + 84 entities × 14 bytes = 1,197 bytes):
60 packets per second ≈ 72 KB/s ≈ 575 kbit/s per client, about 2.3 Mbit/s for 4 clients. This is a theoretical
payload estimate, not a measured bandwidth result. It excludes UDP/IP overhead and retransmitted connection
attempts; the cross-play exercise did not measure this worst-case load. Delta snapshots and a lower send rate with
client interpolation remain network-track work to assess separately.

The 1,200-byte payload cap reduces fragmentation risk; it does not guarantee that every network path can carry
the resulting IP packet without fragmentation. Path MTU and encapsulation still matter. See the
[UDP message-size and reliability guidance in RFC 8085](https://www.rfc-editor.org/rfc/rfc8085.html#section-3.2).

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
**For high scores:** SQLite remains a candidate if durable score storage is added. No database is required
by the current implementation, and this comparison does not add high scores to the delivery scope.

### Accessibility settings and ownership

The [accessibility study](accessibility.md) maps current behavior and tracked features to physical/motor,
visual/audio and cognitive barriers. The action-based input protocol lets the client own key mapping;
it does not implement remapping, toggle auto-fire or a settings file automatically. Those client changes
remain tracked work. A shared game-speed/difficulty setting would additionally need server/gameplay agreement.
Platform/cross-play tests are not an accessibility audit; no compliance certification is claimed.

## 7. Security

### Threat model

The server listens on a public UDP port. Anyone can send it any datagram. The client trusts only the server
it chose to connect to.

| Threat | Mitigation | Status |
|---|---|---|
| Malformed or malicious packet (crash, buffer overflow) | Size, magic, version, payload and enum checks; decoding returns `std::optional` and invalid packets are dropped. Tests cover malformed examples, not every possible input. | Implemented; see `tests/test_protocol.cpp`. |
| Reading past the end of a buffer | `ByteReader` returns 0 and marks itself as failed instead of reading out of bounds. Fields are read one by one, never with `memcpy` of structs. | Implemented; this alone is not a proof of all memory safety properties. |
| Cheating with a modified client | Authoritative server: clients only send input flags. Positions, damage and score are computed on the server. | Implemented in the architecture |
| Too many clients / resource exhaustion | At most 4 players; a client silent for 5 seconds is removed. The player cap does not bound queue growth or packet rate. | Cap and timeout implemented; further bounds remain separate work. |
| Flooding the server with packets | Invalid packets are dropped before any game logic. Rate limiting per address is **planned**. | Partial |
| Spoofed source address (sending inputs as another player) | Not covered: UDP has no authentication. A random session token returned in `ConnectAccepted` and required in every packet is **planned**. | Planned |
| Eavesdropping | Not covered: traffic is not encrypted. The game exchanges no personal data, so the risk is low. | Accepted |

### Vulnerabilities of the technologies

| Technology | Main risk | How we reduce it |
|---|---|---|
| C++ | Memory errors: out-of-bounds access, dangling pointers, use after free | Standard containers (`std::vector`, `std::optional`, `std::span`) instead of raw arrays and `new`; `std::unique_ptr` for ownership; strict warnings (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`); unit tests. |
| UDP | No delivery guarantee, no authentication | Full snapshots tolerate losses; validation of every datagram; session token planned. |
| Third-party libraries (Asio, SFML, GoogleTest) | A vulnerability in a dependency | Version tags are declared in `cmake/Dependencies.cmake` (`asio-1-38-2`, `3.1.0`, `v1.17.0`). Pinning versions supports reproducibility but does not establish that those versions are bug-free. |

Windows clang-tidy still reports three `NewDelete` diagnostics through Asio's thread backend. They are not
classified as harmless and no runtime memory fault was reproduced. The traces remain for investigation with
the network maintainer; no checks were disabled. This is separate from the successful game tests and cross-play.

### Long-term security monitoring

- **Dependencies:** follow the release notes and GitHub security advisories of Asio, SFML and GoogleTest.
  CPM is not supported by Dependabot, so each version bump is a manual pull request.
- **Checks before every push:** `scripts/check.sh` checks formatting, builds with strict warnings, runs clang-tidy
  and the unit tests. The manual CI workflow covers build, tests and formatting; static analysis is a local check.
- **Candidates:** build the tests with AddressSanitizer and UndefinedBehaviorSanitizer, and fuzz the packet
  decoder (for example with libFuzzer), because it processes untrusted network input. These are not implemented checks.

## 8. Summary of trade-offs

| Decision | What we gain | What we accept |
|---|---|---|
| Our own ECS with sparse arrays | Simplicity, testability, full understanding | Some memory on empty slots; no generation counter yet |
| Separate engine modules and installed package | Reuse with selected dependencies and a separate consumer | Additional CMake rules; compatible compiler/OS/configuration still required; no runtime plugins |
| Fixed timestep with catch-up limit | Consistent step duration; bounded catch-up work | Game loses simulated time after a long stall; cross-machine determinism is not guaranteed |
| Event bus with immediate and queued modes | Decoupled systems, safe deferred reactions | Events must be copyable; no priorities yet |
| Full snapshots over UDP | Robust to packet loss, simple | Higher bandwidth than deltas |
| No persistence | No corruption or disk security issues | Nothing survives a server restart |
