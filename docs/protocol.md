# R-Type Network Protocol — version 1

This document describes every message exchanged between `r-type_client` and `r-type_server`.
It should be enough to write a new client. The reference implementation is `common/src/Protocol.cpp`. This is
the protocol specification; runtime gaps are listed in the [networking implementation guide](networking.md).

## 1. Transport

- All game communication uses **UDP**. Default server port: **4242**.
- One datagram contains exactly one packet. Maximum datagram size: **1200 bytes**.
- All integers are **unsigned, big-endian**. `f32` is an IEEE-754 single-precision float sent as its 32-bit pattern, big-endian.

## 2. Packet header (10 bytes)

| Offset | Size | Field | Description |
|---|---|---|---|
| 0 | u16 | magic | Always `0x5254` ("RT") |
| 2 | u8 | version | Protocol version, currently `1` |
| 3 | u8 | type | Message type (see §4) |
| 4 | u32 | sequence | Number of the packet for this peer, see §3.1 |
| 8 | u16 | payload_size | Number of bytes after the header |

## 3. Validation

A receiver **silently drops** a datagram if any of these is true:

- it is shorter than 10 bytes or longer than 1200 bytes;
- `magic` or `version` doesn't match;
- `payload_size` differs from the actual number of bytes after the header;
- `type` is unknown, or the payload size is wrong for this type;
- an enumerated field has an unknown value.
- an `Input` packet sets any reserved flag bit (`0xE0`).

A malformed datagram must never crash the receiver or close the session.

### 3.1 Sequence numbers

Each peer numbers the packets it sends **to each other peer separately**: the server keeps one counter per client,
and a client has one counter towards the server. A counter starts at `0` for a new session, goes up by exactly `1`
for every packet (each part of a split Snapshot is a packet), and wraps around at 2^32. A new session starts when a
client sends `Connect`; a server answering a `ConnectRejected` to an unknown address uses any number.

A receiver remembers the highest number it has seen from a peer and which of the 32 numbers before it arrived.
From that it classifies every packet whose header is valid:

| Verdict | Meaning | What the receiver does |
|---|---|---|
| Newest | higher than every number seen so far (numbers may be missing before it) | handles it |
| Late | lower than the highest, never seen | handles it, except an `Input` or a `Snapshot`, which a newer one has already replaced |
| Duplicate | already received | ignores it |
| Too old | more than 31 numbers behind the highest | ignores it |

A `Connect` always restarts the numbering of its sender: a client that starts again from the same address can number
its packets from `0` again. Ignored packets are counted (`duplicatesDropped`, `outdatedDropped`) but never close the
session. A client **must** increment its number for every packet: a client that always sends the same number has all
its packets after the first treated as duplicates.

## 4. Messages

### 4.1 Client → server

| Type | Name | Payload | Description |
|---|---|---|---|
| `0x01` | Connect | — | Ask to join. Resent every 500 ms until ConnectAccepted or ConnectRejected. |
| `0x02` | Input | `u8 flags` | Keys currently held. Sent every client tick (60/s); also acts as keep-alive. |
| `0x03` | Disconnect | — | The client leaves. |
| `0x04` | Ping | `u32 timestamp_ms` | Latency measurement; the server answers with Pong. The client sends one every second while connected, which also keeps the session alive. |

Input flags: `0x01` up, `0x02` down, `0x04` left, `0x08` right, `0x10` shoot.

### 4.2 Server → client

| Type | Name | Payload | Description |
|---|---|---|---|
| `0x81` | ConnectAccepted | `u8 player_id` | `player_id` in 0–3. |
| `0x82` | ConnectRejected | `u8 reason` | `1` server full, `2` version mismatch. |
| `0x83` | Snapshot | see below | Game state, score and entities at a server tick. |
| `0x84` | EntityDestroyed | `u32 net_id`, `u8 reason` | `1` killed, `2` out of bounds. |
| `0x85` | PlayerLeft | `u8 player_id`, `u8 reason` | `1` disconnected, `2` timed out. |
| `0x86` | Pong | `u32 timestamp_ms` | Same value as the Ping. |

#### Snapshot payload

| Size | Field |
|---|---|
| u32 | tick |
| u8 | state: `1` waiting for players, `2` playing, `3` finished |
| u32 | score |
| u16 | count (max 84) |
| count × 14 bytes | entities |

Each entity:

| Size | Field | Description |
|---|---|---|
| u32 | net_id | Network id of the entity, unique for the whole game |
| u8 | type | `1` player, `2` enemy, `3` player missile, `4` enemy missile |
| u8 | variant | Player index (0–3) for players, subtype for other types |
| f32 | x | World position (world is 1920 × 1080, origin top-left) |
| f32 | y | |

A tick with more than 84 entities is sent as several Snapshot packets with the same `tick`.

The end of the game is not a separate message: the client sees `state = 3` in the snapshots, with the final score.
Since a snapshot is sent every tick, this information cannot be lost.

## 5. Session

1. The client sends **Connect** until it receives **ConnectAccepted** or **ConnectRejected**.
2. The client sends **Input** every tick. The server sends **Snapshot** every tick to every client.
3. The client keeps the entities of the newest tick it has received: the Snapshot packets of one tick are merged
   (duplicates are ignored), a Snapshot of a newer tick replaces them, and one of an older tick is dropped. An
   entity missing from the newest tick therefore disappears, even if its **EntityDestroyed** was lost. The client
   also removes it on **EntityDestroyed**. A new session (**ConnectAccepted**) resets the tick counter.
4. A client that sends nothing for **5 seconds** is considered gone: the server removes its ship and sends
   **PlayerLeft** (reason `2`) to the other clients. A **Disconnect** gives the same result with reason `1`.

## 6. Runtime conformance notes

The protocol defines the intended wire format and message set. The current runtime does not yet implement every
specified behavior: it does not reject a version mismatch with `ConnectRejected` (a datagram with another
version is silently dropped), and message delivery is not guaranteed: only duplicates and reordering are
detected, lost packets are not retransmitted. See the
[networking implementation guide](networking.md) for the current behavior and limitations.
