# Accessibility Study

The subject asks what we provide for players with physical and motor, audio and visual, and mental and
cognitive disabilities. This page lists, for each category, the solutions we chose, why, and which games
inspired them. The accessibility of the documentation itself is covered in the
[documentation guidelines](documentation-guidelines.md).

Status labels: **Implemented**, **Planned (part 1)** for features tracked by an issue to deliver now,
**Planned (part 2)** for later improvements.

## References

- [Game Accessibility Guidelines](https://gameaccessibilityguidelines.com/): a list of recommendations sorted
  into basic, intermediate and advanced.
- [Xbox Accessibility Guidelines](https://learn.microsoft.com/en-us/gaming/accessibility/guidelines): Microsoft's
  guidelines for game developers.
- Existing games: *Celeste* (Assist Mode: slower game speed, invincibility), *The Last of Us Part II*
  (more than 60 accessibility settings, high contrast mode, audio cues), *Fortnite* (visualize sound effects),
  *Overwatch* (filters for each type of color blindness).

## 1. Physical and motor disabilities

A shoot'em up asks for fast and sustained input. The risk is excluding players who cannot press several keys at
once, hold a key for long, or use a standard keyboard.

| Solution | Why | Inspiration | Status |
|---|---|---|---|
| Reconfigurable keys | Players choose keys they can reach, or keys mapped by an adaptive controller | Almost every PC game, Xbox system-level remapping | Planned (part 1, #66) |
| Auto-fire toggle | Holding the fire key during a whole game is tiring; one press turns continuous fire on or off | Many shoot'em ups offer auto-fire | Planned (part 2) |
| Gamepad support | Some adaptive controllers present themselves as gamepads | Xbox Adaptive Controller | Planned (part 2) |
| Slower game speed for the whole game | Less reaction time needed | *Celeste* Assist Mode | Planned (part 2) |

**Design choice that makes this possible:** the client turns keys into *actions* (up, down, left, right, shoot)
before sending them. The protocol only carries actions (`Input` flags), never key codes, so remapping keys or
adding a gamepad changes only the client input code.

**Limit:** the server is authoritative and shared by up to 4 players, so a slower game speed would affect every
player in the game. It must therefore be a game setting, not a personal one.

## 2. Audio and visual disabilities

### Visual

| Solution | Why | Inspiration | Status |
|---|---|---|---|
| Players distinguished by shape and number, not only by color | About 1 man in 12 has some form of color blindness; color alone is not enough | *Overwatch* | Planned (part 1, #43, #67) |
| Enemy missiles different from player missiles by shape and size, not only color | The most important information on screen must stay readable | — | Planned (part 1, #67) |
| High contrast mode: dimmer star field, outlines around ships and missiles | Moving backgrounds make small objects hard to see | *The Last of Us Part II* high contrast mode | Planned (part 1, #67) |
| Large HUD text, readable fonts | Score and lives must be readable from a distance and with low vision | Game Accessibility Guidelines (basic) | Planned (part 1, #46) |
| No fast flashing; explosions without full-screen flashes | Flashing can trigger seizures in photosensitive players | Game Accessibility Guidelines (basic) | Planned (part 1) |

**Design choice that makes this possible:** the server only sends an entity's `type` and `variant`. The client
alone decides how it looks, so color palettes and contrast modes change only the client rendering code.

### Audio

| Solution | Why | Inspiration | Status |
|---|---|---|---|
| Visual cue for every important sound (hit taken, enemy wave, game over) | Deaf and hard of hearing players must get the same information | *Fortnite* visualize sound effects | Planned (part 1, #68) |
| Separate volumes for music and effects, and a mute option | Some players need effects without music, or no sound at all | Most PC games | Planned (part 1, #68) |
| No information given by sound only | Same reason as above; also useful in a noisy room | Game Accessibility Guidelines (basic) | Rule for every new feature |

## 3. Mental and cognitive disabilities

| Solution | Why | Inspiration | Status |
|---|---|---|---|
| Few controls: 4 directions and 1 fire key | Less to remember; the whole game can be learnt in seconds | Classic arcade games | Implemented (protocol) |
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
