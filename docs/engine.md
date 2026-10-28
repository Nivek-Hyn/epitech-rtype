# Engine

`engine/` is a generic 2D game engine library. It knows nothing about R-Type: it only uses the C++ standard
library, so another game could use it unchanged. This page explains what each part does and how to use it.
For step-by-step guides, see the [how-to guides](how-to.md).

| Part | Header | Namespace | Status |
|---|---|---|---|
| Entities and components | `engine/ecs/Entity.hpp`, `engine/ecs/SparseArray.hpp`, `engine/ecs/Registry.hpp` | `engine::ecs` | Implemented |
| Fixed timestep clock | `engine/core/FixedTimestep.hpp` | `engine::core` | Implemented |
| Event bus | `engine/core/EventBus.hpp` | `engine::core` | Implemented |
| Collision shapes | `engine/physics/Aabb.hpp` | `engine::physics` | Implemented |
| Movement system | `engine/systems/Movement.hpp` | `engine::systems` | Implemented |
| Byte serialization | `engine/net/ByteBuffer.hpp` | `engine::net` | Implemented |
| Thread-safe queue | `engine/core/ThreadSafeQueue.hpp` | `engine::core` | Implemented |

All headers are under `engine/include/`. The repository uses the `rtype_engine` umbrella CMake target.
An external project can use the installed `RTypeEngine::Engine` target or select individual modules.
See the [standalone build and installation instructions](../engine/README.md).
The expected behavior of every part is specified by the unit tests in `tests/test_*.cpp`.

## Build boundaries and module selection

The engine requires C++20 and the standard library. It does not include `common/`, SFML or Asio.
The game supplies its own components, networking transport, rendering and rules.

| Installed target | Provides | Links to |
|---|---|---|
| `RTypeEngine::ECS` | Registry and component storage | Standard library |
| `RTypeEngine::Core` | Timing, event bus, thread-safe queue | `Threads::Threads` |
| `RTypeEngine::Physics` | AABB overlap functions | Standard library |
| `RTypeEngine::Net` | Binary buffers, without socket transport | Standard library |
| `RTypeEngine::Systems` | Generic movement | `RTypeEngine::ECS` |
| `RTypeEngine::Engine` | All enabled modules | Each enabled target above |

ECS and Core are static libraries. Physics, Net and Systems are header-only interface targets.
Every target propagates its include directory and C++20 requirement to consumers.
Modules are selected at CMake configuration time, not enabled or unloaded while the game runs.
This is not a dynamic plugin API. Disabling ECS also requires disabling Systems.

For example, a game using only collisions can request the Physics component:

```cmake
find_package(RTypeEngine 0.1 CONFIG REQUIRED COMPONENTS Physics)
target_link_libraries(my_game PRIVATE RTypeEngine::Physics)
```

The [basic consumer](../engine/examples/basic/main.cpp) defines its own Position and Velocity types,
registers the generic movement system and checks the result. Its separate CMake project uses the
installed package rather than the engine's source tree. Run the complete check from the repository root:

```sh
cmake -P engine/cmake/CheckPackage.cmake
```

## 1. Entity-Component-System

### Concepts

- **Entity**: an id (`engine::ecs::Entity`, a `std::uint32_t`). It has no data and no behavior by itself.
  `NullEntity` (the largest value) means "no entity".
- **Component**: any struct holding data, for example `struct Position { float x; float y; };`.
  The engine does not define components: the game does.
- **System**: a function `void(Registry &registry, float timeStepSeconds)` that reads and updates components.

### Registry

The `Registry` owns the entities, their components and the systems.

```cpp
engine::ecs::Registry registry;

engine::ecs::Entity ship = registry.spawn();          // new id (reuses a killed id if possible)
registry.emplace<Position>(ship, 100.f, 540.f);       // add or replace a component
registry.emplace<Velocity>(ship, 300.f, 0.f);

if (Position *position = registry.get<Position>(ship)) // borrow a component, or nullptr
    position->x += 10.f;

registry.has<Velocity>(ship);                          // true
registry.remove<Velocity>(ship);                       // remove one component
registry.kill(ship);                                   // remove every component, free the id
```

`each` visits every live entity that has **all** the listed components:

```cpp
registry.each<Position, Velocity>([](engine::ecs::Entity entity, Position &position, Velocity &velocity) {
    // called once per entity having both a Position and a Velocity
});
```

Systems are registered once and run in registration order by `update`:

```cpp
registry.addSystem([](engine::ecs::Registry &world, float timeStepSeconds) {
    world.each<Position, Velocity>([timeStepSeconds](engine::ecs::Entity, Position &position, Velocity &velocity) {
        position.x += velocity.x * timeStepSeconds;
    });
});
registry.update(1.f / 60.f); // runs every system once
```

### How it is stored

Each component type has its own `SparseArray<C>`: a `std::vector<std::optional<C>>` indexed by entity id.
Slot `i` holds the component of entity `i`, or is empty. Finding a component is a direct index, with no search.

The Registry finds the storage of a type with a `std::unordered_map<std::type_index, std::unique_ptr<ISparseArray>>`.
`ISparseArray` is a small interface that lets `kill` remove an entity from every storage without knowing their types.

### Rules to follow

| Rule | Why |
|---|---|
| Do not keep a pointer or reference to a component after the current system | Adding a component can grow a vector and move every component of that type. |
| Do not keep an entity id across ticks | Killed ids are reused: an old id may now point to a different, alive entity, and `alive` cannot tell (there is no generation counter yet). |
| Never send an `Entity` over the network | Ids are local to each machine. Send a `NetworkId` component instead. |
| Do not add a system from inside a running system | `update` is iterating over the list of systems. |
| Inside `each`, do not kill entities or add components of the visited types | It can invalidate the references passed to the callback. Use `EventBus::enqueue` and react after the systems have run. |

`get<C>` returns a borrowed pointer: it transfers no ownership. Removing or replacing that component,
killing its entity, destroying the Registry, or growing that component's vector can invalidate it,
even within the same system. Read the pointer again after a storage-changing operation.

### Failure behavior

| Operation | Result |
|---|---|
| `spawn` when no usable id remains | Throws `std::overflow_error`. |
| `emplace<C>` on a dead or unknown entity | Throws `std::invalid_argument`. |
| `get<C>` on a dead entity or an absent component | Returns `nullptr`; no storage is created. |
| `kill` on a dead entity, or `remove<C>` on an absent component | Does nothing. |
| `addSystem` with an empty function | Throws `std::invalid_argument`. |

Allocation failures and exceptions thrown by component constructors or systems are not caught by the
Registry. If a system throws during `update`, later systems are not called by that invocation.

## 2. Fixed timestep clock

The simulation always advances by the same duration, whatever the speed of the machine. `FixedTimestep`
converts real elapsed time into a number of fixed steps.

```cpp
engine::core::FixedTimestep clock(60.0);   // 60 steps per second

while (running) {
    for (int ticks = clock.advance(); ticks > 0; ticks--)
        registry.update(clock.step());     // step() = 1/60 s
    std::this_thread::sleep_for(clock.untilNextTick());
}
```

| Function | What it does |
|---|---|
| `advance()` | Adds the time elapsed since the last call, returns how many whole steps to simulate now. |
| `step()` | Duration of one step, in seconds. |
| `untilNextTick()` | How long to sleep before the next step is due (zero if it is already late). |

Catch-up limit: `advance` returns at most 5 steps by default (second constructor argument). If the program was
stopped for longer, the extra whole steps are dropped and only the fraction of a step is kept. Without this
limit, a server that is too slow would accumulate an ever-growing delay. The trade-off is that, after a long
pause, the game runs slightly behind real time.

## 3. Event bus

The event bus lets systems react to each other without knowing each other (Mediator pattern).
An event is any copyable struct.

```cpp
struct Collision { engine::ecs::Entity first; engine::ecs::Entity second; };

engine::core::EventBus bus;
bus.subscribe<Collision>([&](const Collision &collision) {
    // for example: apply damage
});

bus.publish(Collision{missile, enemy});   // handlers run now
bus.enqueue(Collision{missile, enemy});   // handlers run at the next dispatch()
bus.dispatch();                           // deliver every queued event, in order
```

| Function | When to use it |
|---|---|
| `publish` | The reaction must happen immediately and cannot modify what the caller is iterating over. |
| `enqueue` + `dispatch` | Default choice inside systems: events are delivered once all systems have run, so handlers can kill entities safely. |

Handlers of one event type run in subscription order. An event enqueued by a handler during `dispatch`
is delivered in the same `dispatch`. Do not subscribe from inside a handler.

## 4. Physics

### Axis-aligned bounding boxes

```cpp
engine::physics::Aabb ship{100.f, 500.f, 64.f, 32.f};   // left, top, width, height
engine::physics::Aabb missile{150.f, 510.f, 16.f, 8.f};
engine::physics::overlaps(ship, missile);              // true
```

Boxes that only touch along an edge do not overlap. The game builds an `Aabb` from its own position and
hitbox components, then decides what a collision means (damage, score...).

### Movement system

`movement<Position, Velocity>` adds `velocity × timeStepSeconds` to the position of every entity having both
components. It is a template, so the game uses its own component types (each needs `float x` and `float y`):

```cpp
registry.addSystem(engine::systems::movement<rtype::components::Position, rtype::components::Velocity>);
```

Velocity is in world units per second, so movement speed does not depend on the tick rate.

## 5. Networking utilities

These utilities are implemented in engine headers; they do not implement UDP or the R-Type protocol.

- `ByteWriter` / `ByteReader` write and read integers and floats in big-endian, field by field. Reading past the
  end never overflows: the reader returns 0 and `ok()` becomes false.
- `ThreadSafeQueue<T>` passes values from the network thread to the game thread. `tryPop` returns
  `std::nullopt` immediately when the queue is empty after acquiring its mutex. It can wait for that mutex;
  this is not a lock-free queue and it does not wait for a new value to arrive. The queue has no capacity limit.

## 6. Tests

```sh
ctest --test-dir build --output-on-failure -R '^(Registry|FixedTimestep|EventBus|Aabb|Movement)\.'
```

| Test file | What it checks |
|---|---|
| `tests/test_registry.cpp` | Id reuse, components, `each`, systems |
| `tests/test_fixed_timestep.cpp` | Step duration, catch-up limit, waiting time, invalid settings |
| `tests/test_event_bus.cpp` | Immediate and queued delivery, order, chained events |
| `tests/test_aabb.cpp` | Overlap, separation, touching edges |
| `tests/test_movement.cpp` | Movement scaled by the time step, use as a system |

## 7. Known limits and next steps

- Entity ids are reused without a generation counter (see the rules above).
- `each` checks every id; with many more entities, iterating over the smallest storage first would be faster.
- The event bus has no priorities and no unsubscription; they are not needed by the game yet.
