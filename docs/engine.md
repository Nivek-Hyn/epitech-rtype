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
| Byte serialization | `engine/net/ByteBuffer.hpp` | `engine::net` | Designed (network team) |
| Thread-safe queue | `engine/core/ThreadSafeQueue.hpp` | `engine::core` | Designed (network team) |

All headers are under `engine/include/`. Link with the `rtype_engine` CMake target.
The expected behavior of every part is specified by the unit tests in `tests/test_*.cpp`.

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
| Do not keep an entity id across ticks without checking `alive` | Killed ids are reused: an old id may now point to a different entity. |
| Never send an `Entity` over the network | Ids are local to each machine. Send a `NetworkId` component instead. |
| Do not add a system from inside a running system | `update` is iterating over the list of systems. |
| Inside `each`, do not kill entities or add components of the visited types | It can invalidate the references passed to the callback. Use `EventBus::enqueue` and react after the systems have run. |

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

## 5. Networking utilities (designed)

These two parts are specified by their headers and implemented by the network team.

- `ByteWriter` / `ByteReader` write and read integers and floats in big-endian, field by field. Reading past the
  end never overflows: the reader returns 0 and `ok()` becomes false.
- `ThreadSafeQueue<T>` passes values from the network thread to the game thread. `tryPop` never blocks.

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
