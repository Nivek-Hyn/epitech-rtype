# Client

`r-type_client` shows the game and sends the player's actions. It decides nothing about the game: the server
is the authority, and the client draws what the snapshots contain.

## Files

| File | Role |
|---|---|
| `client/src/main.cpp` | Command-line arguments, then the application |
| `client/src/App.cpp` | One state per screen (menu, campaign map, editor, game); hosts the local server; F1/F2/F3; where Escape leads |
| `client/src/ui/CampaignMap.cpp` | Campaign map: one node per level, locked levels, difficulty, progress saved in `config.json` |
| `client/src/editor/LevelEditor.cpp`, `LevelEditorDraw.cpp` | Level editor inside the game, on an engine `Registry` |
| `client/src/render/WorldBackground.cpp` | Background of each world: image layers listed in `assets/config/backgrounds.json`, each scrolling at its own speed |
| `client/src/ui/MainMenu.cpp` | Menu screens: join a server, settings (volumes, auto fire, high contrast, key remapping) |
| `client/src/ui/GameConfig.cpp` | Settings saved in `config.json`; an invalid file falls back to the defaults |
| `client/src/game/Session.cpp` | One game: inputs sent, snapshots received and smoothed, effects of destroyed entities |
| `client/src/game/SessionHud.cpp` | HUD: score, wave, lives of every player, banners, waiting room, end screen |
| `client/src/render/EntityRenderer.cpp` | One drawing function per entity type and variant |
| `client/src/net/AsioNetworkClient.cpp` | UDP connection on its own thread (see [networking](networking.md)) |
| `client/src/ResourceManager.cpp` | Textures, font, music and sounds loaded from `assets/` |

## One frame

```mermaid
flowchart LR
    Keys[Keyboard] -->|Input flags| Net[AsioNetworkClient]
    Net -->|Snapshot packets| Assembler[SnapshotAssembler]
    Assembler --> Interpolator[SnapshotInterpolator]
    Interpolator -->|due snapshots| Effects[Session: effects, HUD state]
    Interpolator -->|blended entities| Renderer[EntityRenderer]
```

*Every frame, the session sends the held actions, merges the snapshot packets of each tick, then handles the
snapshots whose time has come (sounds, explosions, HUD) and draws the entities blended between two snapshots.*

1. `Session::sendInput` turns the held keys into input flags. With auto fire, Shoot is sent without holding the key.
   Without the window focus, an empty input is sent, so another window's keys never move the ship.
2. `Session::receive` merges the packets of a tick (`SnapshotAssembler`) and queues the complete snapshot in the
   `SnapshotInterpolator`, which shows the world about two snapshots in the past.
3. A snapshot whose time has come updates the score, lives, level, wave and game state.
4. An `EntityDestroyed` from the server says why an entity disappeared. Only a killed one explodes (with its sound,
   flash or screen shake); a bonus that was picked up or an entity that left the screen does not. The entity is hidden
   at once, until the snapshots drop it too. A `PlayerLeft` shows "P2 LEFT THE GAME" for a few seconds.
5. `Session::draw` draws the blended entities, then the HUD.

## Drawing an entity

The server sends a type and a variant; `EntityRenderer::draw` chooses the drawing function from the type. The
variant comes from the `Kind` of the entity's prefab in `assets/config/entities.json`.

| Type | Function | Variants |
|---|---|---|
| Player | `drawPlayer` | Player index: ship color, "P1"..."P4" tag; tilt follows vertical movement |
| Player missile | `drawPlayerMissile` | `0` blaster, `1` laser (longer, magenta) |
| Enemy missile | `drawEnemyMissile` | `0` to `4`: shape, color and animation of each enemy shot |
| Enemy | `drawEnemy`, `drawScout` | Sprite table `enemyLook`: texture, frame, scale and vertical offset of each variant |
| Obstacle | `drawObstacle` | `0` debris, `1` Martian rock, `2` singularity shard; colored squares without the texture |
| Bonus | `drawBonus` | Orb with a letter: `S` spread, `L` laser, `+` life |

To show a new enemy variant, add its entry to `enemyLook` (`client/src/render/EntityRenderer.cpp`) and use the
variant in the prefab. An unknown variant is drawn with the default boss sprite.

## Settings and accessibility

Settings are read from `config.json` at startup and written when the client closes or a setting changes.
Remapped keys must all differ; a conflict resets the controls to the defaults. Auto fire (F1) and high contrast (F2)
can be toggled in game. High contrast removes the starfield and the screen shake, softens the damage flash,
outlines the HUD text and circles every enemy shot. See the [accessibility study](accessibility.md).

## Leaving a game

Escape returns to the menu. A refused connection, an unreachable server or a lost connection also returns to the
menu, with a message explaining why. F3 shows the network statistics: ping, bandwidth, loss, dropped and resent packets.
