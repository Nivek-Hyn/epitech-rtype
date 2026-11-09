# 0011 — Delta snapshots evaluated, not adopted

**Status:** accepted (revisit if the game gets many static entities)

## Context
Delta snapshots are the usual answer to a large snapshot: the server remembers the last snapshot a client
acknowledged and sends only what changed since. The subject lists them among the techniques to study. Before building
one, we measured what a snapshot of this game really contains.

## Measurement
A scripted client played a campaign game for 40 seconds (holding the fire key, moving up and down), received every
snapshot and compared each with the previous tick, with positions rounded to 1/16 of a unit:

- 27.8 entities per snapshot on average (38 at most), 415 bytes per packet with the first protocol.
- **98 % of the entities moved at every tick** (2 % unchanged, 0.4 % new). Missiles, enemies, obstacles and tiles all
  fly at some speed; only a ship that nobody moves stands still.
- Estimated bytes per tick: 390 with full 14-byte entities, 278 with full 10-byte entities, and **278 again** if only
  the changed entities were sent (the same, because nearly all of them changed).
- Coding a position change on 2 bytes instead of 4 is possible only if it is small: all the observed steps between
  two consecutive ticks fit in one signed byte (1/16 unit). But the server can only compute a difference against a
  snapshot the client acknowledged, which is several ticks old on a real network (one round trip at 60 ticks per
  second is 3 to 8 ticks), and a missile at 400 units per second moves 6.7 units per tick: after 2 ticks the step
  already exceeds a signed byte (8 units). The saving disappears as soon as there is latency.

## Options
| Option | Result |
|---|---|
| Delta against the last acknowledged snapshot, changed entities only | Saves nothing here: nearly every entity changed |
| Delta with the position difference on a small integer | Saves on a local network only; falls back to full positions on a real one |
| **Send fewer snapshots, smaller entities, interpolate** ([0010](0010-snapshot-rate-and-interpolation.md)) | −65 % measured, works whatever the latency |
| Send velocities and let the client extrapolate | Would save more, but the enemy patterns (sine waves, zigzags) are not straight lines: an extrapolation error would be visible, and the client would repeat the server's movement rules |

## Decision
Delta snapshots are not implemented. Their complexity (a history of snapshots per client, acknowledgement of ticks,
resynchronization after a loss, more tests) buys nothing measurable on this game, while the options of 0010 give most
of the saving for little code. The tools to check this again are in the repository: `r-type_server --stats`, the F3
overlay and `scripts/netsim.py`.

## Consequences
- A snapshot stays self-contained: a client that joins late or loses packets needs nothing to recover.
- If a future game mode has many entities that stay still (fixed turrets, a large map), the measurement above should
  be repeated before reconsidering.
