# 0006 — ECS with sparse arrays

**Status:** accepted

## Context
The game needs a way to represent many kinds of objects (ships, enemies, missiles...) without deep
inheritance trees, and to keep systems (movement, rendering, collisions) independent from each other.

## Options
| Option | Pros | Cons |
|---|---|---|
| Inheritance (`Entity` → `Ship` → `Player`) | Familiar | Rigid, "diamond" problems, logic and data mixed |
| **ECS, sparse arrays** | Simple, fast enough, easy to explain | Some memory wasted on holes in the arrays |
| ECS, archetypes | Best cache efficiency | Much more complex to implement |
| Third-party ECS (EnTT...) | Mature | The engine is what we must build ourselves |

## Decision
Our own ECS (`engine/include/engine/ecs/`):
- **Entity** = an index (`std::uint32_t`), recycled when killed.
- **Component** = a plain struct with data only.
- **SparseArray<C>** = `std::vector<std::optional<C>>`, indexed by entity.
- **System** = a function `void(Registry&, float dt)`, run in registration order.
- `Registry::each<A, B>(f)` calls `f(entity, a, b)` for every entity having both A and B.

## Consequences
- Adding a feature = adding components and a system, without touching existing classes.
- Each machine has its own Registry: entity indexes are local. Entities are identified over the network
  by the `NetworkId` component, never by their index.
