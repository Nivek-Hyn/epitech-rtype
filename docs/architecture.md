# Architecture Overview

This page gives the big picture: which subsystems exist, how they depend on each other, and where they live
in the code. The reasons behind each choice are in the [technical study](technical-study.md) and the
[architecture decision records](adr/README.md).

Each diagram is followed by a text description, so the page can be read without seeing the diagrams.

## 1. Layers

The subject requires visible and decoupled subsystems for rendering, networking and game logic.
The code is split into four modules, and dependencies only go one way.

```mermaid
flowchart BT
    engine["engine/<br/>ECS, game loop, physics, events, serialization"]
    common["common/<br/>protocol, shared components, constants"]
    server["server/<br/>game logic + server network"]
    client["client/<br/>rendering, input, audio + client network"]
    common --> engine
    server --> common
    client --> common
```

*Text description: `engine` is at the bottom and depends on nothing but the standard library.
`common` depends on `engine`. `server` and `client` both depend on `common` (and therefore on `engine`),
but never on each other.*

| Module | Responsibility | Third-party libraries | CMake target |
|---|---|---|---|
| `engine/` | Generic game engine: knows nothing about R-Type | none | `rtype_engine` (static library) |
| `common/` | What the server and the client must agree on: protocol, components, constants | none | `rtype_common` (static library) |
| `server/` | Authoritative game simulation and server networking | Asio | `rtype_server_lib` (game logic, tested) + `r-type_server` |
| `client/` | Display, keyboard input, sound and client networking | SFML, Asio | `r-type_client` |

Rules enforced in code review ([CONTRIBUTING](https://github.com/EpitechPGE3-2026/G-ING-500-MAR-5-1-rtype-5/blob/main/CONTRIBUTING.md)):
- `engine/` never includes anything from `common/`, `server/` or `client/`.
- SFML is only used in `client/`. Asio is only used in network implementation files.

## 2. Subsystems and where they are in the code

```mermaid
flowchart LR
    subgraph Engine["engine/"]
        ECS["ECS<br/>Registry, SparseArray"]
        Loop["Game loop<br/>FixedTimestep"]
        Physics["Physics<br/>Aabb, movement"]
        Events["Events<br/>EventBus"]
        Bytes["Serialization<br/>ByteWriter, ByteReader"]
        Queue["Threading<br/>ThreadSafeQueue"]
    end
    subgraph Common["common/"]
        Protocol["Protocol<br/>encode / decode"]
        Components["Components<br/>Position, Velocity, NetworkId, Kind"]
    end
    subgraph Server["server/"]
        Game["Game logic<br/>Game"]
        ServerNet["Network<br/>INetworkServer"]
    end
    subgraph Client["client/"]
        Render["Rendering<br/>SFML"]
        ClientNet["Network<br/>INetworkClient"]
    end
    Protocol --> Bytes
    Game --> ECS
    Game --> Components
    ServerNet --> Protocol
    ServerNet --> Queue
    ClientNet --> Protocol
    Render --> ECS
```

*Text description: the engine offers six independent building blocks. The protocol uses the engine's
serialization. The server game logic uses the ECS and the shared components. Both network layers use the
protocol; the server network layer also uses the thread-safe queue. The client renders entities stored in its
own ECS.*

| Subsystem | Main files | Status |
|---|---|---|
| ECS | `engine/include/engine/ecs/` (`Entity.hpp`, `SparseArray.hpp`, `Registry.hpp`), `engine/src/Registry.cpp` | Implemented |
| Game loop | `engine/include/engine/core/FixedTimestep.hpp`, `engine/src/FixedTimestep.cpp` | Implemented |
| Physics | `engine/include/engine/physics/Aabb.hpp`, `engine/include/engine/systems/Movement.hpp` | Implemented |
| Events | `engine/include/engine/core/EventBus.hpp` | Implemented |
| Serialization | `engine/include/engine/net/ByteBuffer.hpp` | Designed |
| Threading | `engine/include/engine/core/ThreadSafeQueue.hpp` | Designed |
| Protocol | `common/include/common/Protocol.hpp`, `common/src/Protocol.cpp`, [protocol.md](protocol.md) | Designed |
| Shared components | `common/include/common/Components.hpp`, `common/include/common/Constants.hpp` | Implemented |
| Game logic | `server/src/Game.hpp`, `server/src/Game.cpp` | Designed |
| Server network | `server/src/net/INetworkServer.hpp` | Designed |
| Client network | `client/src/net/INetworkClient.hpp` | Designed |
| Rendering | `client/src/main.cpp` | Window only |

"Designed" means the header (the contract) and the unit tests exist; the implementation is in progress.

## 3. Client–server model

The server is the only authority on the game. Clients send what the player presses; the server sends back
the whole state of the game, 60 times per second.

```mermaid
sequenceDiagram
    participant Client
    participant Server
    Client->>Server: Connect (repeated until answered)
    Server-->>Client: ConnectAccepted (player id 0-3)
    loop every tick (60 per second)
        Client->>Server: Input (keys held)
        Server-->>Client: Snapshot (game state, score, entities)
    end
    Server-->>Client: EntityDestroyed / PlayerLeft (when they happen)
    Client->>Server: Disconnect
```

*Text description: the client sends `Connect` until the server answers `ConnectAccepted` with a player id.
Then, every tick, the client sends its `Input` and the server sends a `Snapshot`. The server also sends
`EntityDestroyed` and `PlayerLeft` when they happen. The client sends `Disconnect` when it leaves.*

Why it works over UDP: a lost `Snapshot` is replaced by the next one 16.7 ms later, so the client never stays
out of sync. The end of the game is part of the snapshot (`state = finished`), so it cannot be lost.
Full specification: [protocol.md](protocol.md).

## 4. Server: one tick

```mermaid
flowchart TD
    Start(["clock.advance() returns N steps"]) --> Poll["Read network events<br/>network.poll()"]
    Poll --> Apply["Apply them to the game<br/>onPlayerJoined / onPlayerLeft / onInput"]
    Apply --> Update["Run the systems<br/>game.update(clock.step())"]
    Update --> Send["Send the state<br/>broadcast(snapshot), events"]
    Send --> More{"Another step?"}
    More -- yes --> Poll
    More -- no --> Sleep["Sleep until the next step<br/>clock.untilNextTick()"]
    Sleep --> Start
```

*Text description: each loop asks the clock how many fixed steps are due. For each step, the server reads
the network events, applies them to the game, runs every system once, then sends the snapshot and events.
When no step is left, it sleeps until the next one is due.*

This loop is in `server/src/main.cpp`. The game logic (`Game`) never touches sockets: `main` gives it the
network events and sends what it produces, so it can be unit tested without a network.

## 5. Threads

```mermaid
flowchart LR
    subgraph NetworkThread["Network thread (Asio)"]
        Receive["Receive and decode datagrams"]
        Transmit["Encode and send datagrams"]
    end
    subgraph GameThread["Game thread"]
        Simulation["Fixed-step simulation"]
    end
    Receive -- "ThreadSafeQueue<br/>(events)" --> Simulation
    Simulation -- "send / broadcast<br/>(thread-safe)" --> Transmit
```

*Text description: on the server, the network thread receives and decodes datagrams and pushes the resulting
events into a thread-safe queue. The game thread reads that queue at the start of each step without ever
waiting. The game thread sends messages through thread-safe `send` and `broadcast` functions.*

The client follows the same idea: its network thread fills a queue that the main (rendering) thread reads
every frame. The game logic therefore never waits for the network, and a slow or silent client cannot block
the simulation.

## 6. Inside the engine: the ECS

```mermaid
flowchart LR
    Registry["Registry"] --> Alive["alive ids<br/>vector&lt;bool&gt;"]
    Registry --> Free["free ids<br/>vector&lt;Entity&gt;"]
    Registry --> Storages["component storages<br/>map: type → SparseArray&lt;C&gt;"]
    Registry --> Systems["systems<br/>vector&lt;function&gt;"]
    Storages --> PositionArray["SparseArray&lt;Position&gt;<br/>[pos0, empty, pos2, ...]"]
    Storages --> VelocityArray["SparseArray&lt;Velocity&gt;<br/>[vel0, empty, empty, ...]"]
```

*Text description: the `Registry` owns four things: which ids are alive, which ids can be reused, one storage
per component type, and the list of systems. Each storage is a `SparseArray`, a vector indexed by entity id
where a slot is either a component or empty.*

- An **entity** is only an id (`std::uint32_t`). It has a component when the slot at its index is filled.
- A **system** is a function `void(Registry &, float timeStepSeconds)`. `Registry::update` runs them in the order
  they were added.
- `Registry::each<Position, Velocity>(function)` calls the function for every live entity having *all* the
  listed components.

The engine is documented in more detail in [engine.md](engine.md).
