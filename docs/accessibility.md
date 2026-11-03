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
| Remappable actions | Lets players choose reachable keys | Tracked, #66 | Rebind each action, handle conflicts and confirm saved settings after restart. |
| Toggle auto-fire | Avoids holding a key for a long period | Tracked, #114 | One press toggles; stopping/reconnecting does not leave unwanted shooting enabled. |
| Gamepad/adaptive controller input | Alternative input devices | Optional | Select supported hardware and verify it before claiming support. |
| Slower game speed | More reaction time | Optional | An authoritative multiplayer speed setting affects the whole game and needs team agreement. |

The client sends action flags, not physical key codes. This allows a different local mapping without
changing the movement actions understood by the server. It does not mean remapping or device support is already present.

## Visual access

| Feature | Barrier addressed | Current status | Verification to perform |
|---|---|---|---|
| Player appearance variants | Helps identify players | Available as sprite variants; shape/number redundancy is not established | Check four players without relying only on hue. |
| Shapes/numbers in addition to color | Supports recognition without color alone | Tracked, #67 | Check players and opposing projectile types in normal play and contrast modes. |
| High-contrast presentation | Helps distinguish objects from the background | Tracked, #67 | Compare ships/projectiles, backgrounds, overlays and both UI themes where applicable. |
| Readable HUD text | Helps read score, health and states | HUD tracked, #46 | Check text size, contrast, scaling and absence of clipping. |
| Controlled flashes and distractions | Reduces distracting effects | Review needed when animations/effects are added | Inspect the delivered effects; this prototype is not a photosensitivity safety assessment. |

The current prototype has a dark background and sprites. That alone does not establish suitable contrast
for every object or an implemented high-contrast option. The server sends type/variant; the client owns their appearance.

## Audio access

| Feature | Barrier addressed | Current status | Verification to perform |
|---|---|---|---|
| Visual alternatives to important sounds | Lets players recover information with sound muted | Tracked, #68, alongside sound work #44 | Play muted and verify damage, wave and game-state information remains available visually. |
| Separate volumes and mute | Lets players control competing sound | Tracked, #68 | Check music/effects independently and verify persistence where settings are implemented. |
| Subtitles/captions | Makes spoken content available as text | Optional; no spoken content is implemented here | Review actual content before adding a subtitle requirement. |

No gameplay sound is integrated in this snapshot. Silence is not evidence that the future sound/visual
alternatives are complete; review both together when those features are integrated.

## Cognitive access

| Feature | Barrier addressed | Current status | Verification to perform |
|---|---|---|---|
| Few actions: movement and shooting | Limits the number of controls to learn | Available | Ask another member to start without verbal instructions. |
| Controls printed in the terminal | Provides a basic reference | Available; not an in-game help screen | Ensure the in-game UI makes controls discoverable when #45 is implemented. |
| Connection, waiting and end-game screens | Explains what to do and the current state | Tracked, #45 and #47 | Verify defaults, errors, reconnect flow, victory and defeat. |
| Clear state and score display | Makes outcomes understandable | HUD tracked, #46; wire fields already exist | Check actual state transitions and the client display, not just enum declarations. |
| Difficulty options | Adjusts challenge | Optional | Coordinate actual gameplay settings and test how they affect all players. |

## Evidence and remaining work

The two-player Linux/Windows tests established connection, rendering and observed gameplay; they were not
user testing with disabled players or a formal accessibility audit. See the [validation record](validation.md).
There are no accessibility pass results for the unimplemented tracked features above.

Before marking one of those issues complete, record the implementation revision, the setting/control path,
the platform, the expected outcome and an actual test result. Revisit this page after integrating client,
gameplay and audio changes. Text descriptions and status words must remain usable without interpreting colors.
| Solution | Why | Inspiration | Status |
|---|---|---|---|
| Few actions: 4 directions and 1 fire action; arrows or ZQSD move, Space fires | Less to remember; the whole game can be learnt in seconds | Classic arcade games | Implemented (client and protocol) |
| Controls shown on the waiting screen | Players do not have to remember them or read a manual | Game Accessibility Guidelines (basic) | Planned (part 1, #45) |
| Clear game states: waiting, playing, finished, each with its own screen | The player always knows what is happening and what to do next | — | Planned (part 1, #45, #47) |
| Simple connection screen with default values (address and port prefilled) | Fewer steps and fewer chances of error | — | Planned (part 1, #45) |
| Consistent visual language: same shape and color meaning everywhere | Reduces what the player has to interpret | Game Accessibility Guidelines (basic) | Rule for every new feature |
| Difficulty settings (enemy count and speed) | Lets players with slower processing enjoy the game | *Celeste* Assist Mode | Planned (part 2) |

## 4. Summary

| Category | Part 1 | Part 2 |
|---|---|---|
| Physical and motor | Reconfigurable keys | Auto-fire, gamepad, game speed |
| Visual | Shapes in addition to colors, high contrast, large HUD, no flashing | Color blindness filters |
| Audio | Visual cues, separate volumes | Subtitles for any spoken content |
| Cognitive | Few controls, controls on screen, clear game states | Difficulty settings |

The architecture helps: input is sent as actions and the server never decides how things look, so most
accessibility features only touch the client and never change the protocol or the game logic.
