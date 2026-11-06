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
- `assets/config/entities.json`: enemy archetypes (health, movement pattern, attacks).
- `assets/config/game_rules.json`: difficulty multipliers, infinite mode growth, campaign order.

Runtime loading and scaling are implemented in:

- `server/src/Game.cpp`

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

- Vertical dives and parabolic movement.
- Faster punish windows and stronger cross-lane pressure.

Main archetypes:

- `martian_hopper`
- `gravity_diver`
- `dust_striker`
- Boss: `boss_mars_excavator`

### World 3: Singularity Frontier

Fantasy:

- Distorted spacetime near a singularity core.

Gameplay identity:

- Erratic movement and spiral area denial.
- Multi-layer pattern combinations that require anticipation.

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

- Stage 7 (`level7.json`): adaptation to erratic and spiral movement.
- Stage 8 (`level8.json`): pattern stacking and spacing discipline.
- Stage 9 (`level9.json`): final pre-boss stress, then singularity core boss.

## Difficulty model

Difficulty is configured in `assets/config/game_rules.json` under `difficulties`.

Current multipliers:

- `easy`: lower enemy health, speed, fire rate pressure, bullet speed, score.
- `normal`: baseline values.
- `hard`: increased enemy health, speed, fire pressure, bullet speed, score.

At spawn time, final combat stats are derived from base archetype values and level values.

- Final health = base health x difficulty health multiplier x infinite health growth
- Final speed = base speed x difficulty speed multiplier x infinite speed growth
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
- Ensure new enemy archetypes exist in `entities.json`.
- Validate JSON syntax and run a full campaign pass.