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
   | Client only (drawing, animation...) | a header in `client/src/`, namespace `rtype::client` |

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

## Add an enemy, a tile or a bonus

Game entities are **prefabs**: `assets/config/entities.json` lists each entity with its components.
The server creates the entity from that list; its systems produce the game. No C++ is needed when the new
entity only combines existing components.

```json
"armored_scout": {
  "Kind": {"type": "enemy", "variant": 1},
  "Hitbox": {"width": 64, "height": 56},
  "Health": {"hp": 4},
  "Velocity": {"x": -90.0},
  "MovementPattern": {"type": "sine_wave", "amplitude": 120},
  "EnemyAttack": [{"fire_rate": 1.5, "bullet_speed": 380.0, "bullet_pattern": "zigzag"}],
  "EnemyTraits": {"score": 150}
}
```

| Component | Fields | Used by |
|---|---|---|
| `Kind` | `type` (`player`, `enemy`, `player_missile`, `enemy_missile`, `obstacle`, `bonus`), `variant` | Snapshot: the client chooses how it looks from both |
| `Hitbox` | `width`, `height`, `offset_y` | Collisions and screen limits; no Hitbox means no collision |
| `Health` | `hp` | Damage from missiles |
| `Velocity` | `x`, `y` (world units per second; negative x goes left) | Movement |
| `MovementPattern` | `type`, `amplitude`, `frequency`, `hold_x` | Enemy movement patterns (`server/src/MovementSystem.cpp`) |
| `EnemyAttack` | list of `fire_rate`, `bullet_speed`, `bullet_variant`, `bullet_pattern`, `offset_y` | Enemy shooting |
| `EnemyTraits` | `score`, `is_boss` | Score in infinite mode; a boss survives contact with a ship |
| `MissileDamage` | `damage` | Health a player missile removes |
| `BonusEffect` | `weapon`, `lives` | What a player gets by touching the bonus |
| `PlayerControl` | `speed` | Speed of a ship moved by inputs |

Then use the name in a level (`"type": "armored_scout"`) and restart the server.
A prefab with an unknown component or invalid data is skipped with a warning at startup.
A new `variant` also needs its drawing in the client (`client/src/render/EntityRenderer.cpp`, names in `render/Looks.hpp`); reusing an existing variant needs nothing.

### Add a component usable in prefabs

1. Define the struct (see [Add a component](#add-a-component)).
2. Register it under a name in `registerGameComponents` (`server/src/Prefabs.cpp`), with a function reading it
   from JSON:

   ```cpp
   prefabs.registerComponent<Shield>("Shield", [](const Json &json) { return Shield{json.value("hits", 1)}; });
   ```

3. Write or extend the system that uses it. The engine's `Prefabs` class does not know JSON: the same
   mechanism works with any data format.

## Add a weapon

Weapons are in `assets/config/weapons.json`. A weapon names the prefab of its missile, its cooldown, and one
angle per missile fired at once:

```json
"twin": { "missile": "player_missile", "cooldown": 0.3, "angles": [-4.0, 4.0] }
```

The missile prefab gives its variant (how it looks), hitbox, speed and `MissileDamage`. To give the weapon to players,
add a bonus prefab with `"BonusEffect": {"weapon": "twin"}` and list it in `bonus_drops.prefabs`.
`bonus_drops.chance` is the probability that a destroyed enemy drops a bonus. A destroyed ship goes back to
`default_weapon`.

## Add a configured wave

The campaign plays the levels listed in `assets/config/game_rules.json`, under `campaign.levels`.
Start the server from the repository root or the installed game directory. Restart it after editing a file.

Append an entry to the `waves` array of a level, keeping valid JSON commas:

```json
{
  "wave_id": 3,
  "delay_before_start": 10.0,
  "enemies": [
    { "type": "patrol", "spawn_y": 500, "speed": 120 }
  ],
  "obstacles": [
    { "type": "satellite_debris", "spawn_y": 200 }
  ]
}
```

`type` is a prefab name. The wave fires once, when the level has been played for ten seconds: the delay is
measured from the beginning of the level, not from the previous wave. `spawn_y` is a world coordinate
(a negative value picks a random height). `speed`, `health`, `amplitude` and `frequency` replace the values
of the prefab for this enemy only; 0 or nothing keeps them.

### Edit a level with the level editor

In the client menu, choose **CREATE A LEVEL**. The level is shown as it is played: the world background, the
enemies as they look in the game, time running to the right (one line per second) and the height of the screen from top to bottom.

| Action | Control |
|---|---|
| Choose the enemy or tile to place | Click it in the palette, or Tab / Shift+Tab |
| Place it | Left click on the timeline |
| Move it | Drag it |
| Delete it | Right click on it, or Delete while the mouse is over it |
| Move in time | Mouse wheel, or Left / Right arrows |
| Save | SAVE or Ctrl+S |
| Play it for real | TEST: saves, starts a local server on this level and joins it; Escape comes back to the editor |
| Open another level | OPEN goes through `assets/levels/custom.json` (the default) and the campaign levels |
| Change the background | WORLD |

The file keeps every key the editor does not show (amplitude, health...). Placements in the same quarter of a
second share a wave. The editor's objects are entities of an engine `Registry` (`client/src/editor/`); reading and
writing the files is in `tools/level-editor/`.

## Change the network protocol

1. Update `common/include/common/Protocol.hpp` **and** [protocol.md](protocol.md) in the same pull request.
2. Increase `rtype::protocol::Version` if old clients can no longer understand the packets.
3. Add a round-trip test (encode then decode) and a malformed-packet test in `tests/test_protocol.cpp`.
