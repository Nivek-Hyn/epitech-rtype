# Accessibility study and current status

This page separates available behavior, tracked delivery work and optional ideas. It describes the code
in the combined local work, not unmerged gameplay/rendering branches. The documentation has its own
[accessibility guidelines](documentation-guidelines.md).

- **Available**: visible in the code or observed during the recorded tests; this is not a full accessibility certification.
- **Tracked**: a delivery issue exists, but the feature is not implemented in this snapshot.
- **Optional**: an idea for later; it is not an additional delivery promise.

The [Xbox Accessibility Guidelines](https://learn.microsoft.com/en-us/xbox/accessibility/guidelines)
provide design and test guidance, including input, text, contrast, extra visual/audio channels and UI context.
Earlier project research listed Celeste, The Last of Us Part II, Fortnite and Overwatch as inspiration;
that research does not establish that their accessibility features exist in this project.

## Physical and motor access

| Feature | Barrier addressed | Current status | Verification to perform |
|---|---|---|---|
| Keyboard movement with arrows or ZQSD | Gives two existing key layouts | Available; keyboard input only | Check both layouts, diagonal movement and release on Linux/Windows. |
| Continuous shooting while holding Space | Avoids repeated presses | Available through the Shoot action and server cooldown; still requires holding a key | Check held fire and release; distinguish this from toggle auto-fire. |
| Remappable actions | Lets players choose reachable keys | Available in Settings, saved in `config.json` | Rebind each action, handle conflicts and confirm saved settings after restart. |
| Toggle auto-fire | Avoids holding a key for a long period | Available: Settings or F1 in game, saved; shown in the HUD. No action is sent while the window has no focus | One press toggles; stopping/reconnecting does not leave unwanted shooting enabled. |
| Gamepad/adaptive controller input | Alternative input devices | Optional | Select supported hardware and verify it before claiming support. |
| Slower game speed | More reaction time | Optional | An authoritative multiplayer speed setting affects the whole game and needs team agreement. |

The client sends action flags, not physical key codes. This allows a different local mapping without
changing the movement actions understood by the server. Gamepads are not supported.

## Visual access

| Feature | Barrier addressed | Current status | Verification to perform |
|---|---|---|---|
| Player appearance variants | Helps identify players | Available: sprite variants plus a "P1"..."P4" tag above each ship and in the HUD | Check four players without relying only on hue. |
| Shapes/numbers in addition to color | Supports recognition without color alone | Available: player tags, bonus letters (S, L, +), red ring around enemy shots in high contrast | Check players and opposing projectile types in normal play and contrast modes. |
| High-contrast presentation | Helps distinguish objects from the background | Available: Settings or F2 in game, saved. Plain dark background, outlined HUD text | Compare ships/projectiles, backgrounds, overlays and both UI themes where applicable. |
| Readable HUD text | Helps read score, health and states | Score, wave and lives of every player shown | Check text size, contrast, scaling and absence of clipping. |
| Controlled flashes and distractions | Reduces distracting effects | High contrast softens the red damage flash and removes the screen shake | Inspect the delivered effects; this prototype is not a photosensitivity safety assessment. |

These options were not measured against a contrast ratio and are not a photosensitivity assessment.
The server sends type/variant; the client owns their appearance.

## Audio access

| Feature | Barrier addressed | Current status | Verification to perform |
|---|---|---|---|
| Visual alternatives to important sounds | Lets players recover information with sound muted | Available: explosions, red damage flash, wave banner, "P2 LEFT THE GAME" banner, end screens | Play muted and verify damage, wave and game-state information remains available visually. |
| Separate volumes and mute | Lets players control competing sound | Available: music and effects volumes (with mute) in Settings, saved | Check music/effects independently and verify persistence where settings are implemented. |
| Subtitles/captions | Makes spoken content available as text | Optional; no spoken content is implemented here | Review actual content before adding a subtitle requirement. |

Music, shots, hits and explosions are played; every one of them also has a visual counterpart.

## Cognitive access

| Feature | Barrier addressed | Current status | Verification to perform |
|---|---|---|---|
| Few actions: movement and shooting | Limits the number of controls to learn | Available | Ask another member to start without verbal instructions. |
| Controls shown in game | Provides a basic reference | Available: the waiting room lists the controls | Ensure the in-game UI makes controls discoverable when #45 is implemented. |
| Connection, waiting and end-game screens | Explains what to do and the current state | Available: connection messages (refused, unreachable, lost), waiting room with ready state, victory and game over screens | Verify defaults, errors, reconnect flow, victory and defeat. |
| Clear state and score display | Makes outcomes understandable | Available: score, level, wave and lives of every player in the HUD | Check actual state transitions and the client display, not just enum declarations. |
| Difficulty options | Adjusts challenge | Available: easy, normal, hard when hosting (and `--difficulty` on the server) | Coordinate actual gameplay settings and test how they affect all players. |

## Evidence and remaining work

The two-player Linux/Windows tests established connection, rendering and observed gameplay; they were not
user testing with disabled players or a formal accessibility audit. See the [validation record](validation.md).
Gamepads, game speed and subtitles are not implemented.

Before marking one of those issues complete, record the implementation revision, the setting/control path,
the platform, the expected outcome and an actual test result. Revisit this page after integrating client,
gameplay and audio changes. Text descriptions and status words must remain usable without interpreting colors.

| Solution | Why | Inspiration | Status |
|---|---|---|---|
| Few actions: 4 directions and 1 fire action; arrows or ZQSD move, Space fires | Less to remember; the whole game can be learnt in seconds | Classic arcade games | Implemented (client and protocol) |
| Controls shown on the waiting screen | Players do not have to remember them or read a manual | Game Accessibility Guidelines (basic) | Implemented |
| Clear game states: waiting, playing, finished, each with its own screen | The player always knows what is happening and what to do next | — | Implemented |
| Simple connection screen with default values (address and port prefilled) | Fewer steps and fewer chances of error | — | Implemented |
| Consistent visual language: same shape and color meaning everywhere | Reduces what the player has to interpret | Game Accessibility Guidelines (basic) | Rule for every new feature |
| Difficulty settings (enemy health, speed and fire rate) | Lets players with slower processing enjoy the game | *Celeste* Assist Mode | Implemented |

## 4. Summary

| Category | Implemented | Not implemented |
|---|---|---|
| Physical and motor | Reconfigurable keys, auto fire | Gamepad, game speed |
| Visual | Shapes and letters besides colors, high contrast, softer flashes | Color blindness filters |
| Audio | Visual cues for every sound, separate volumes | Subtitles (no spoken content) |
| Cognitive | Few controls, controls on screen, clear game states, difficulty settings | — |

The architecture helps: input is sent as actions and the server never decides how things look, so most
accessibility features only touch the client and never change the protocol or the game logic.
