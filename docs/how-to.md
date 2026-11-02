# How-To Guides

Short recipes for the most common changes. They assume you know the concepts from the
[engine documentation](engine.md): entities, components, systems and the event bus.
Before changing data shared across domains, read the [shared data reference](shared-data.md).

## Run the component and system example

`engine/examples/basic/` is a complete consumer project. It defines its own components and adds a system
without changing the engine. It requires a C++20 compiler, CMake 3.24+ and a build tool, with no SFML,
Asio, game assets or GoogleTest. From the repository root:

```sh
cmake -S engine -B build/engine-tutorial -DCMAKE_BUILD_TYPE=Debug
cmake --build build/engine-tutorial --config Debug --parallel 4
cmake --install build/engine-tutorial --config Debug --prefix "$PWD/build/engine-tutorial-install"
cmake -S engine/examples/basic -B build/engine-tutorial-consumer -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$PWD/build/engine-tutorial-install"
cmake --build build/engine-tutorial-consumer --config Debug --parallel 4
ctest --test-dir build/engine-tutorial-consumer -C Debug --output-on-failure --verbose
```

These commands also work in PowerShell; use a Developer PowerShell for MSVC and keep `--config Debug`
and `-C Debug` for Visual Studio generators. Use matching engine/consumer compilers and configurations.
The test prints `Custom component and system: movement, lifetime and deferred destruction passed`.
Alternatively, `cmake -P engine/cmake/CheckPackage.cmake` builds, installs and tests the example in one command.

Read `engine/examples/basic/main.cpp` in this order:

1. `Lifetime` holds the remaining lifetime in seconds. The struct belongs to the consumer; the Registry
   needs no new registration or code change to store it.
2. `lifetimeSystem` receives the Registry and the duration of one simulated step. Its `each<Lifetime>`
   callback visits only live entities with that component and subtracts the step duration.
3. Expired ids are collected in a vector. The following loop kills those entities after `each` has finished,
   so none of its component references is accessed after destruction.
4. `main` registers movement first, then lifetime. Each `update(0.5f)` runs both systems in that order.
   The temporary entity starts with one second of lifetime; the other entity has no Lifetime component.
5. After the first update, both entities have moved and the temporary entity has half a second left.
   After the second, the temporary entity and all its components are removed; the permanent one survives.
   Fresh pointers are obtained after the systems run.

The two updates represent simulated time; this console example does not sleep for a real second.
To experiment, change the initial lifetime to `1.5f` and adjust the checks to expect half a second remaining
after the two updates. Keep the engine unchanged: the new behavior belongs to the consumer.

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

An enemy is an entity, not a class. Game.cpp currently spawns enemies and checks player-missile collisions;
the client displays an enemy sprite. A new subtype combines component values, a behavior and a rendering choice.
Changing `Kind::variant` alone does not add a behavior or a new sprite mapping.

1. **Pick a variant number.** `Kind{EntityType::Enemy, variant}` identifies the enemy type for the client.
   Keep the list of variants in one place (a comment or an enum in `common/`), so the server and the client
   agree on it.

2. **Spawn it on the server** with the components every networked entity needs, plus its own behavior:

   ```cpp
   const std::uint8_t variant = 1;
   engine::ecs::Entity enemy =
       spawnNetworked(protocol::EntityType::Enemy, variant, rtype::WorldWidth - EnemyWidth, 300.f);
   _registry.emplace<rtype::components::Velocity>(enemy, -200.f, 0.f);
   // + server-only components: health, hitbox, movement pattern, shooting...
   ```

   This snippet belongs inside a Game method: `spawnNetworked` is private, not a free function callable by
   arbitrary code. It already gives the entity NetworkId, Kind, Position and a zero Velocity; the example
   replaces Velocity. The snapshot sends NetworkId, Kind and Position, not every component automatically.

3. **Give it a behavior.** Reuse existing systems when possible (movement, shooting). For a new movement pattern,
   add a server-only component holding its parameters and a system that updates Velocity. Run that behavior
   before movement if the current tick must use the new velocity. Keep resource/render code outside the server.

4. **Display it on the client.** The current enemy branch uses one texture rectangle regardless of variant.
   Add an explicit variant-to-sprite mapping and test its fallback. Reusing type/variant needs no new wire
   field; adding health or other state requires the shared-contract review described above.

5. **Test the server side** like any system: spawn the enemy, run a few ticks, check its position or state.

### Add a configured wave

Game loads `assets/levels/level1.json` at startup through `server/src/levelLoader.cpp`.
Start the server from the repository root or the installed game directory. Restart it after editing the file.
The other supplied level files are not selected automatically: campaign progression remains separate work.

Append an entry to the top-level `waves` array, keeping valid JSON commas:

```json
{
  "wave_id": 3,
  "delay_before_start": 10.0,
  "enemies": [
    { "type": "patrol", "spawn_y": 500, "speed": 120 }
  ]
}
```

The wave fires once when accumulated active simulation time reaches ten seconds. Time advances only
while at least one player has a live ship. Delay is measured from the level's beginning, not from the
previous wave. `spawn_y` is a world coordinate; positive `speed` moves the enemy left from the right edge.
For the current 72-unit-high enemy, keep y between 0 and 1008 in the 1080-unit-high world.

The loader reads `type`, `amplitude` and `frequency`, but the integrated spawner currently uses only y
and speed: changing those other fields does not implement a new movement pattern. Missing files,
malformed JSON and a missing/non-array `waves` field are rejected; not every numeric range or optional
field shape is validated yet. Keep finite, nonnegative delays and sensible positions/speeds.

Connect a client and verify the extra enemy appears at the configured active time and height.
This is an extension recipe, not evidence that another team member has already followed it.

Use configuration for values an existing behavior already consumes (timing, count, speed or positions).
A new behavior still needs C++ unless a scripting mechanism capable of expressing it is actually implemented.

## Change the network protocol

1. Update `common/include/common/Protocol.hpp` **and** [protocol.md](protocol.md) in the same pull request.
2. Increase `rtype::protocol::Version` if old clients can no longer understand the packets.
3. Add a round-trip test (encode then decode) and a malformed-packet test in `tests/test_protocol.cpp`.
