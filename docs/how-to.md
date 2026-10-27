# How-To Guides

Short recipes for the most common changes. They assume you know the concepts from the
[engine documentation](engine.md): entities, components, systems and the event bus.

## Add a component

A component is a plain struct holding data, with default values.

1. **Choose where it goes**, depending on who needs it:

   | Used by | File |
   |---|---|
   | Any game (generic) | Nowhere: the engine never defines components. Make the engine code a template instead (see `engine/systems/Movement.hpp`). |
   | Server **and** client | `common/include/common/Components.hpp`, namespace `rtype::components` |
   | Server only (health, hitbox, AI...) | a header in `server/src/`, namespace `rtype::server` |
   | Client only (sprite, animation...) | a header in `client/src/`, namespace `rtype::client` |

2. **Write the struct.** Use explicit names and put the unit in a comment when there is one:

   ```cpp
   // Rectangle used for collisions, relative to the entity's Position, in world units.
   struct Hitbox {
       float width = 0.f;
       float height = 0.f;
   };
   ```

3. **Use it.** No registration is needed: the Registry creates the storage the first time the type is used.

   ```cpp
   registry.emplace<Hitbox>(ship, 64.f, 32.f);
   ```

Do not put functions with game logic in a component: logic belongs in systems.

## Add a system

A system is a function `void(engine::ecs::Registry &, float timeStepSeconds)`.

1. **Write the function.** It usually iterates over the entities having the components it needs:

   ```cpp
   // Removes entities that left the screen on the left side.
   void outOfBoundsSystem(engine::ecs::Registry &registry, float /* timeStepSeconds */)
   {
       std::vector<engine::ecs::Entity> toKill;
       registry.each<rtype::components::Position>([&](engine::ecs::Entity entity, rtype::components::Position &position) {
           if (position.x < -100.f)
               toKill.push_back(entity);
       });
       for (engine::ecs::Entity entity : toKill)
           registry.kill(entity);
   }
   ```

   Entities are collected first and killed after `each`: killing inside `each` would invalidate the
   references the loop is using.

2. **Register it** once, in the right order. On the server, systems are registered in the `Game` constructor
   (`server/src/Game.cpp`). They run in registration order every tick, so register a system after the systems
   whose results it needs (for example collisions after movement).

   ```cpp
   _registry.addSystem(engine::systems::movement<rtype::components::Position, rtype::components::Velocity>);
   _registry.addSystem(outOfBoundsSystem);
   ```

3. **Test it** in `tests/`, without a window or a socket: create a `Registry`, add a few entities, call the
   system once, check the components.

   ```cpp
   TEST(OutOfBounds, KillsEntitiesLeftOfTheScreen)
   {
       engine::ecs::Registry registry;
       const engine::ecs::Entity enemy = registry.spawn();
       registry.emplace<rtype::components::Position>(enemy, -200.f, 0.f);

       outOfBoundsSystem(registry, 1.f / 60.f);

       EXPECT_FALSE(registry.alive(enemy));
   }
   ```

   Add the test file to `tests/CMakeLists.txt`.

## Make systems communicate with events

Use the event bus when a system must trigger something it should not know about (damage, score, sound).

1. **Define the event**, a copyable struct, next to the system that sends it:

   ```cpp
   struct Collision {
       engine::ecs::Entity first;
       engine::ecs::Entity second;
   };
   ```

2. **Send it** from the system with `enqueue`, so nothing reacts while the system is still iterating:

   ```cpp
   bus.enqueue(Collision{missile, enemy});
   ```

3. **React to it** by subscribing once, at startup:

   ```cpp
   bus.subscribe<Collision>([&](const Collision &collision) {
       // apply damage, add score...
   });
   ```

4. **Deliver the events** once per tick, after `registry.update(...)`:

   ```cpp
   registry.update(timeStepSeconds);
   bus.dispatch();
   ```

## Add an enemy type

An enemy is an entity, not a class: a new enemy type is a new combination of components and a new value of
`Kind::variant`. The current prototype already spawns a basic enemy on a timer, moves it left, and lets player
missiles destroy it. The game loop and renderer are in place; extending enemy behavior still requires server and
client changes.

1. **Pick a variant number.** `Kind{EntityType::Enemy, variant}` identifies the enemy type for the client.
   Keep the list of variants in one place (a comment or an enum in `common/`), so the server and the client
   agree on it.

2. **Spawn it on the server** with the components every networked entity needs, plus its own behavior. The
   current implementation is in `Game::spawnEnemies()` and `Game::spawnNetworked()`:

   ```cpp
   engine::ecs::Entity enemy = spawnNetworked(protocol::EntityType::Enemy, variant, x, y);
   _registry.emplace<rtype::components::Velocity>(enemy, -200.f, 0.f);
   // + server-only components: health, hitbox, movement pattern, shooting...
   ```

   `spawnNetworked` gives the entity a `NetworkId`, `Kind`, `Position` and `Velocity`, which are used by the
   snapshot and movement system. The current spawn timer, enemy rows and speed are constants in `Game.cpp`.

3. **Give it a behavior.** Reuse existing systems when possible (movement, shooting). For a new movement pattern,
   add a component holding its parameters and a system that updates the `Velocity` from it.

4. **Display it on the client.** Add or load a texture through `client/src/ResourceManager.cpp`, then map the
   entity type/variant to a sprite in `client/src/main.cpp`. The protocol does not change if the existing
   `EntityType::Enemy` and `variant` fields are sufficient; a new entity category requires a protocol update.

5. **Test the server side** like any system: spawn the enemy, run a few ticks, check its position or state.

## Change the network protocol

1. Update `common/include/common/Protocol.hpp` **and** [protocol.md](protocol.md) in the same pull request.
2. Increase `rtype::protocol::Version` if old clients can no longer understand the packets.
3. Add a round-trip test (encode then decode) and a malformed-packet test in `tests/test_protocol.cpp`.
