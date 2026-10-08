# 0007 — Authoritative server, inputs up and full snapshots down

**Status:** accepted (part 1; to be revisited in part 2)

## Context
The server must have the final authority on the game. UDP may lose, duplicate or reorder packets.

## Options
| Option | Pros | Cons |
|---|---|---|
| **Clients send inputs, server sends full state every tick** | Simple, self-healing: a lost packet is fixed by the next one | Uses more bandwidth |
| Server sends only events / changes | Low bandwidth | A single lost packet desynchronizes the client forever without a reliability layer |

## Decision
- Client → server: an `Input` message (bit flags) every client tick. It also acts as a keep-alive.
- Server → client: a `Snapshot` every server tick (60/s) with the game state (waiting / playing / finished),
  the score and every networked entity, plus event messages (`EntityDestroyed`, `PlayerLeft`) for things
  the client must react to immediately.
- Anything that must not be lost (like the end of the game) goes in the snapshot rather than in a one-time message.
- The client creates an entity the first time it sees its `NetworkId` in a snapshot, and removes it on
  `EntityDestroyed` or when it hasn't appeared in any snapshot for `EntityStaleTimeout`.

## Consequences
- The server game logic (`server/src/Game`) does not know about sockets: `main` feeds it network events
  and sends what it produces. It can be unit tested alone.
- Part 2 candidates: delta snapshots, lower send rate + interpolation, client-side prediction.
