# Campaign Game Design

This page documents how campaign content was designed and authored for R-Type.
It is intended for game designers, level designers, and programmers who need a shared reference.

## Scope and goals

The campaign currently contains 9 stages split into 3 worlds.

Design goals:

- Build a clear learning curve from readable patterns to high-pressure mixed patterns.
- Give each world a distinct movement identity.
- Keep data-driven balancing through JSON only (no code changes needed for tuning).
- Reuse the same enemy definition system for both campaign and infinite mode.

## Data sources

Campaign and balancing rely on these files:

- `assets/levels/level1.json` to `assets/levels/level9.json`: stage content and wave composition.
- `assets/config/entities.json`: prefabs, each entity described by its components (health, movement pattern, attacks...).
- `assets/config/game_rules.json`: difficulty multipliers, infinite mode growth, campaign order.

Runtime loading and scaling are implemented in:

- `server/src/GameConfigLoader.cpp`: reading the configuration files.
- `server/src/GameLevels.cpp`: going from one level to the next.
- `server/src/GameWaves.cpp`: making enemies and tiles appear.
- `server/src/GameDifficulty.cpp`: difficulty and infinite mode scaling.
- `server/src/GamePlayers.cpp`: controls, lives and respawn.
- `server/src/GameCollisions.cpp`: what happens when a shot or a ship touches something.
- `server/src/Game.cpp`: one step of the game.

## World pillars

### World 1: Orbital Space

Fantasy:

- Satellite traffic, orbital loops, relay platforms.

Gameplay identity:

- Clean trajectories and readable lanes.
- Introduction to mixed pressure with scouts and relays.

Main archetypes:

- `satellite_scout`
- `orbital_probe`
- `solar_relay`
- Boss: `boss_orbital_carrier`

### World 2: Martian Gravity

Fantasy:

- Low orbit over red canyons and unstable gravity wells.

Gameplay identity:

- Low gravity: enemies move like astronauts, with slow falls and bounces on the ground.
- `martian_hopper` bounces between its `spawn_y` and the ground: `spawn_y` sets the height of its jumps.
- `gravity_diver` flies flat, then falls faster and faster and leaves through the bottom of the screen.
- The excavator boss slams the ground between its `spawn_y` and the bottom of the screen.

Main archetypes:

- `martian_hopper`
- `gravity_diver`
- `dust_striker`
- Boss: `boss_mars_excavator`

### World 3: Singularity Frontier

Fantasy:

- Distorted spacetime near a singularity core.

Gameplay identity:

- A black hole at the center of the screen attracts everything: the middle is the dangerous place.
- Everything stays predictable, so that the player can learn it. There is no random movement.
- `quantum_shifter` keeps crossing the central line (`gravity_pull`).
- `vortex_drone` spirals around the center, then is thrown to the left along the central line (`black_hole_orbit`).
- `gravity_well` bullets curve toward the central line.
- The core boss stops in the middle of the screen.

Main archetypes:

- `quantum_shifter`
- `vortex_drone`
- `singularity_fragment`
- Boss: `boss_singularity_core`

## Stage-by-stage design intent

### World 1

- Stage 1 (`level1.json`): onboarding for orbital behavior, low stress, lane recognition.
- Stage 2 (`level2.json`): first sustained mixed-wave pressure with relays.
- Stage 3 (`level3.json`): world exam, then orbital carrier boss.

### World 2

- Stage 4 (`level4.json`): introduction to gravity and parabolic threats.
- Stage 5 (`level5.json`): denser gravity dives and lane collapse moments.
- Stage 6 (`level6.json`): pre-boss pressure, then martian excavator boss.

### World 3

- Stage 7 (`level7.json`): adaptation to the attraction of the black hole.
- Stage 8 (`level8.json`): pattern stacking and spacing discipline.
- Stage 9 (`level9.json`): final pre-boss stress, then singularity core boss.

## Authoring rules

Spawn heights use the full 1080-unit world: `spawn_y` goes from about 100 to 900.
Keep `gravity_diver` in the upper part, since it falls until it leaves the screen.

Components of an enemy prefab in `entities.json` (the full list is in the [how-to guides](../how-to.md#add-an-enemy-a-tile-or-a-bonus)):

- `EnemyTraits.score`: points given when it is destroyed, in infinite mode only (default 100). The campaign has no score.
- `MovementPattern.amplitude`, `MovementPattern.frequency`: tune its movement pattern. A level can override them per enemy; 0 or nothing keeps the default of the pattern.

The speed is read from the level, or from the `Velocity` of the prefab when the level does not give one.

Meaning of `amplitude` and `frequency` in the world 2 and 3 patterns:

| Pattern | `amplitude` (default) | `frequency` (default) |
|---|---|---|
| `martian_bounce` | gravity (150) | not used |
| `gravity_dive` | gravity (300) | seconds before the fall (5) |
| `gravity_pull` | strength of the attraction (1) | not used |
| `black_hole_orbit` | rotation speed (220) | not used |

An enemy that goes below the screen is removed.

## Destructible tiles

A wave can list tiles under `obstacles`, next to `enemies`, with a `type` and a `spawn_y` (from 0 to 1008). Types are prefabs of `entities.json` whose `Kind` type is `obstacle`: `Kind.variant` (appearance), `Health.hp` (shots to destroy it) and `Velocity.x` (negative: to the left). A level can override `speed` and `health`.

| Type | World | Health | Speed |
|---|---|---|---|
| `satellite_debris` | 1 | 2 | 110 |
| `mars_rock` | 2 | 4 | 90 |
| `singularity_shard` | 3 | 3 | 140 |

Rules:

- A tile scrolls to the left and leaves the screen. Levels do not wait for tiles.
- Touching a tile costs a life. The tile stays in place.
- A player shot removes 1 health and gives no score.
- A tile stops enemy shots without being damaged, so players can take cover behind it.
- Difficulty and infinite mode scale the health and speed of tiles like those of enemies.

Always leave a gap of at least 200 units: tiles add to the entities sent in each snapshot, and a wall must not force the player to shoot.

## Boss rules

A boss is an enemy prefab with these fields in `entities.json`:

- `EnemyTraits.is_boss`: touching the boss destroys the ship, not the boss.
- `MovementPattern.hold_x`: the boss stops at this position and fights there. It also never leaves the screen vertically.

A boss therefore has to be shot down: its level does not end while it is alive.

## Lives

Configured in `assets/config/game_rules.json` under `player`:

- `lives`: lives of each player.
- `respawn_delay`: seconds before the ship comes back.
- `invulnerability`: seconds without damage after a respawn.
- `lobby_delay`: seconds the waiting room stays open after the first player joins (5). During that time the
  snapshots carry the `Waiting` state: ships can move, no wave comes, and the client shows the waiting room.
  When every player leaves, the game goes back to the waiting room at the first level.

Lives are per world: `levels_per_world` in the `campaign` section (3 by default) says how many levels form a world. At the start of a new world, every connected player is back to the full count, not added to what is left (2 lives left gives 3, not 5), and a player out of lives comes back.

A player out of lives watches the others until the next world or the end of the game. When every connected player is out of lives, the game is over: enemies disappear and the game over screen is shown. Once all players have left, the next game starts again from the first level, with no score.

## Difficulty model

Difficulty is configured in `assets/config/game_rules.json` under `difficulties`.

Current multipliers:

- `easy`: lower enemy health, speed, fire rate pressure, bullet speed, score.
- `normal`: baseline values.
- `hard`: increased enemy health, speed, fire pressure, bullet speed, score.

At spawn time, final combat stats are derived from base archetype values and level values.

- Final health = base health x difficulty health multiplier x infinite health growth
- Final speed = level speed (or base speed) x difficulty speed multiplier x infinite speed growth
- Final attack cadence uses a lower bound with `min_fire_rate`
- Final bullet speed uses an upper bound with `max_bullet_speed`

## Infinite mode behavior

Infinite mode uses the campaign stage list as a loop.
When the last stage is cleared, it restarts from stage 1 and increments loop count.

Configured growth per loop in `assets/config/game_rules.json`:

- `health_growth_per_loop`
- `speed_growth_per_loop`
- `fire_rate_growth_per_loop`
- `bullet_speed_growth_per_loop`
- `score_multiplier_per_loop`

This gives a predictable long-term progression without changing authored level JSON.

## Extension checklist

When adding a new world:

- Add 3 new stage files.
- Keep stage N+2 as the world boss stage.
- Add paths to `campaign.levels` in order.
- Ensure new enemy prefabs exist in `entities.json`.
- Validate JSON syntax and run a full campaign pass.