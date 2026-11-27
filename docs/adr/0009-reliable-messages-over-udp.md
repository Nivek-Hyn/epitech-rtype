# 0009 — Reliable messages over UDP with acknowledgements

**Status:** accepted

## Context
The server sends the whole game state every tick, so a lost snapshot is replaced by the next one
([0007](0007-authoritative-snapshots.md)). A few messages are different: `EntityDestroyed`, `PlayerLeft` and
`Disconnect` happen once, and nothing replaces them. If one is lost, a destroyed ship stays on a client until the
next snapshot, a departure goes unnoticed, and a server keeps a client that left for the whole 5 second timeout.
The subject also asks that some messages ("connection to a game instance, a player is dead") are reliable.

## Options
| Option | Pros | Cons |
|---|---|---|
| **Acknowledgements and repeats on UDP, for a few messages** | One socket, no head-of-line blocking, cost only where needed | We write and test the mechanism |
| A TCP connection next to UDP | Reliability for free | A second connection to set up, firewalls, and the subject only tolerates it with a strong justification |
| Send every event several times | Trivial | Wasteful, and still not guaranteed |
| Make everything reliable (ack every packet) | Uniform | Acknowledging snapshots and inputs doubles the packets for nothing: a newer one replaces them |

## Decision
- A reliable message has a **reliable number** (per peer, from 0) and a flag bit in the packet type. The receiver
  answers with an `Ack` and never handles the same number twice. The sender repeats the message, in a new packet,
  until the `Ack` arrives.
- The delay before a repeat follows the measured round-trip time (as TCP does, RFC 6298), starts at 200 ms and
  doubles after each repeat; the sender gives up after 12 sends. A sample is taken only from a message sent once,
  because an `Ack` cannot be matched to one of several copies.
- At most 32 reliable numbers are in flight, so that the receiver's 32-number window can always tell a repeat
  from a new message. Reliable messages are not ordered: the game events are independent, and ordering would let a
  lost message block the later ones.
- Reliability is a property of the packet, not of a message type: the logic lives in a generic `ReliableQueue` in the
  engine, without any socket, so it is tested with a simulated clock.
- Version of the protocol goes from 1 to 2.

## Consequences
- `Snapshot`, `Input`, `Ping` and `Pong` stay unreliable. A reliable `Snapshot` is not supported (it would also have
  to be split).
- When the player closes the window, `disconnect()` waits up to 500 ms for the `Ack` of its `Disconnect`.
- Delta snapshots (planned) can rely on `EntityDestroyed` and on the acknowledgements to know what a client has.
- Each reliable message costs one more small packet (the `Ack`, 14 bytes), and more traffic when the network is bad;
  `--stats` and F3 show the number of repeats.
