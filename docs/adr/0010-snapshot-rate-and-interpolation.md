# 0010 — Fewer snapshots, positions on 16 bits, smoothed by the client

**Status:** accepted

## Context
The first protocol sent a full snapshot at every tick (60 per second), with two 32-bit floats per entity: 14 bytes per
entity. On a real campaign game (about 27 entities per snapshot) one client received 24.3 KB/s. The subject asks for
mechanisms against low bandwidth and for real measurements, and names client-side interpolation with low-frequency
server updates among the techniques.

## Options
| Option | Pros | Cons |
|---|---|---|
| Keep 60 full snapshots per second | Lowest delay (16 ms), nothing to smooth | The most bandwidth |
| **30 snapshots per second, 16-bit positions, interpolation on the client** | About 65 % less bandwidth; smooth motion even when the network is irregular; a lost snapshot is hidden | The world is drawn about 100 ms late instead of 16 ms |
| Send less often, no interpolation | Same savings | Entities visibly move in steps of 2 or 3 frames |
| Delta snapshots | See [0011](0011-delta-snapshots-not-adopted.md) | |
| Client-side prediction of the own ship | Removes the extra delay for the player's ship | The client would repeat the server's movement rules (speed, limits) and needs reconciliation: not done |

## Decision
- A position is a `u16` counting 1/16 of a world unit, from −1024 to +3071.94 (clamped). The error is at most 1/32 of a
  unit. An entity costs 10 bytes. Protocol version 7.
- The server sends a snapshot every `SnapshotEveryTicks` = 2 ticks. `--snapshot-every N` overrides it, which made it
  possible to measure 1, 2 and 3.
- The client keeps the snapshots in a `SnapshotInterpolator`: it shows the world `InterpolationDelaySnapshots` = 2
  sending intervals in the past and blends the two snapshots around that time. The intervals are measured, not
  assumed, so the same client works with any `--snapshot-every`.
- Snapshots are *handled* (sounds, explosions, HUD) when their time comes, so effects and drawing stay in step.
- Losses are counted from the gaps in the per-client packet numbers, not from tick numbers, which now skip.

## Consequences
- Measured on the same scripted game: 24.3 KB/s (version 6, 60 per second) → 17.6 KB/s (16-bit positions) → 8.4 KB/s
  (30 per second). At 3 ticks: 5.8 KB/s.
- Measured on a simulation with 30 ms of network jitter: the largest jump of an entity between two frames drops from
  26.7 to 8.4 units (the real speed is 6.7 per frame).
- Reaction time goes from about 16 ms to about 100 ms after the server. The player's own ship is also delayed. This
  is the price of the saving, and why the interval stays configurable.
- Nothing is extrapolated: if no newer snapshot arrives, entities pause.
