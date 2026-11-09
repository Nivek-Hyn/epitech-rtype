# Delivery and defense guide

Use this page to coordinate the remaining work and prepare a repeatable demonstration. It records
acceptance criteria, not new features or deadlines. Follow the owner shown in each GitHub issue;
this page does not reassign another member's work.

## What is delivered

| Area | Where to see it |
|---|---|
| Standalone engine: ECS, prefabs, fixed timestep, event bus, AABB, network building blocks | [Engine](engine.md); `cmake -P engine/cmake/CheckPackage.cmake` |
| Second game made with the engine only | `./brickrush` ([engine page](engine.md#a-second-game-brick-rush)) |
| Data-driven content: prefabs, levels, weapons, rules | `assets/config/*.json`, `assets/levels/*.json` ([how-to guides](how-to.md)) |
| Campaign of 9 levels in 3 worlds, infinite mode, difficulties, bosses, tiles, weapons and bonuses | [Campaign game design](GameDesign/game-design-campaign.md) |
| Waiting room, then playing, victory or game over, decided by the server | `lobby_delay` in `game_rules.json`; snapshot `state` ([protocol](protocol.md)) |
| Level editor in the game, with the real sprites and a Test button | Menu: CREATE A LEVEL ([how-to](how-to.md#edit-a-level-with-the-level-editor)) |
| Reliable UDP protocol: acknowledgements, duplicates, split snapshots, quantization, 30 Hz with interpolation, rate limits | [Protocol](protocol.md), [networking](networking.md), [network guide](network-guide.md) |
| Accessibility: remappable keys, auto fire, high contrast, letters and tags besides colors | Settings screen, F1/F2 in game ([accessibility](accessibility.md)) |

All new work must be reviewed at a named revision. A successful test of a private branch is not evidence
that its changes are present in `main`. Do not close an issue merely because its documentation exists.

## Demonstration scenarios

Run every command from the repository root (or the installed game directory) after a Release build.

1. **The engine is reusable.** Start `./brickrush`: a brick breaker built from the same Registry, prefabs,
   movement system, AABB and event bus, without `common/`. Show `games/brickrush/assets/bricks.json`: its entities
   are described by components, like R-Type's.
2. **The campaign.** `./r-type_client`, CAMPAIGN: the map shows the three worlds, the locked levels and the
   difficulty. Play a level: the background and the HUD show its world; reaching a level unlocks it on the map.
3. **Four players and the waiting room.** `./r-type_server 4242 --stats`, then up to four
   `./r-type_client <host> 4242` on Linux and Windows. Each player presses Space when ready; a fifth client is
   refused ("the server is full"). When a player leaves, the others see "P2 LEFT THE GAME".
4. **Content without recompiling.** Change the `Health` or `Velocity` of a prefab in `assets/config/entities.json`,
   or place enemies in the level editor (CREATE A LEVEL) and press TEST to play the level at once.
5. **A bad network.** `python3 scripts/netsim.py --listen 5000 --server 127.0.0.1:4242 --loss 10 --delay 80 --jitter 30 --dup 2`,
   connect a client to port 5000 and press F3: the game stays smooth, losses and resent packets are counted.
6. **The protocol by hand.** Decode the two datagrams of [protocol §7](protocol.md#7-example-datagrams): header,
   big-endian fields, quantized positions. Explain why UDP, why the server keeps the authority, and how losses,
   duplicates and split snapshots are handled.
7. **Accessibility.** In Settings, remap a key and enable auto fire and high contrast; in game, F1 and F2 toggle
   them. Restart the client: the settings are kept.

## Before the demonstration

1. Select the actual delivery branch/revision and record `git rev-parse HEAD`.
2. Build it with the documented prerequisites, then run [quality checks](static-analysis.md) and tests.
   Keep tool versions, command output and exit statuses; use CTest's `--no-tests=error`.
3. Run `python scripts/build_docs.py` with the documented tools. Check the API and guides through the local site.
4. Build/package the intended configuration, extract outside the source tree and use that directory for the game.
5. Keep the server's host/UDP port and each terminal's role written down. Start one server for each scenario.
   Do not switch host roles while another member is still checking the previous scenario.

The [build guide](build-and-run.md) has exact Linux/Windows commands. The [validation record](validation.md)
contains the existing results and limits, including the earlier qualitative stuttering report.

## Acceptance checklist

These are remaining acceptance actions; unchecked items are not implied failures or successful tests.

- [ ] From the selected revision, run the installed client/server on Linux and native Windows.
- [ ] Exercise a mixed-OS four-player game; test the fifth-player response and disconnect/reconnect behavior.
- [ ] Hold fire and create the planned maximum entity load; verify packet limits and snapshot completeness.
- [ ] Play the delivered campaign through victory and defeat, including the boss and player damage/death rules.
- [ ] Exercise packet loss, latency, duplication and reordering with the network owner's procedure; record measurements.
- [ ] Verify each delivered accessibility setting and saved preference using its issue's acceptance test.
- [ ] Close clients normally and use Ctrl+C in each server console; record exit status rather than closing the terminal.
- [ ] Check Brick Rush: startup, controls, restart and actual engine reuse.
- [ ] Have another domain member follow the engine and content guides from the documented prerequisites.
- [ ] Make sure every member can explain the common protocol and the main architectural decisions.

An additional Windows machine without development tools is a useful distribution check when available.
It has not been tested and is not replaced by hiding tools from PATH on the development PC.
Windows-specific follow-up was deferred; resume it deliberately instead of repeating already successful tests.

## Questions to prepare together

| Question | Evidence or explanation to show |
|---|---|
| What makes the engine reusable? | Build engine/ alone, install it and run the separate consumer; explain which data belongs to the game. |
| Why modules rather than one library or dynamic plugins? | Show the selected targets, their dependencies and current limits in the technical study. |
| What happens when an entity dies and its id is reused? | Show component cleanup and id-reuse tests; explain the lack of a generation counter. |
| Who owns a component returned by get? | Registry/storage retains ownership; the pointer is borrowed and can be invalidated by storage changes. |
| What happens when the server is too slow? | Explain the fixed step, accumulated time, catch-up cap and deliberately dropped simulation time. |
| Why does the client send actions? | Server authority and input flags; explain remapping without confusing local keys and wire values. |
| Why not send a C++ struct directly? | Padding/representation and byte order; show encode/decode and a round-trip test. |
| What happens if a packet is lost or late? | Distinguish replaceable state, discrete events, sequence rules and snapshot completion; acknowledge unfinished work. |
| What is actually accessible today? | Demonstrate delivered settings and the status table, not only the design plan. |
| What did the Windows tests prove? | Native build, tests, archives and two-client cross-play; distinguish unmeasured performance and open shutdown/analysis checks. |

Answer using actual code and observed behavior. If a claim requires a measurement or a teammate's change,
get that evidence before presenting it as a result.

## Evidence to retain

For each scenario record the revision, OS/compiler versions, build configuration, archive checksum,
host/client roles, commands, expected behavior, actual behavior and exit codes. Include entity/player
counts and network conditions for measurements. A screenshot can show rendering; it cannot by itself
prove smooth motion, correct timing or graceful shutdown.

Store raw local logs, transfers and generated files outside versioned source (for example under ignored
build/). Update the public validation page with concise results, retaining open checks and failures.
Never commit credentials, local absolute user paths or a report that labels an unexecuted check successful.
