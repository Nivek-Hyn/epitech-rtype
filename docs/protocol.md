# R-Type Network Protocol — version 8

**Status of this document.** Specification of the R-Type game protocol, version 8, as implemented by the
delivered client and server. Its presentation follows the conventions of the Internet RFCs; it is not an IETF
standard.

**Abstract.** R-Type clients and an authoritative server exchange binary datagrams over UDP. Clients send the
actions held by the player; the server sends the state of the whole game 30 times per second. A few messages that
nothing else replaces are acknowledged and repeated. This document is enough to write a new client without reading
the C++: the reference implementation is `common/src/Protocol.cpp`, and the example datagrams of §7 are checked by
the unit tests (`tests/test_protocol.cpp`). Runtime details are in the [networking implementation guide](networking.md).

**Conventions.** The key words **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT** and **MAY** are to be
interpreted as described in [RFC 2119](https://www.rfc-editor.org/rfc/rfc2119) and
[RFC 8174](https://www.rfc-editor.org/rfc/rfc8174) when, and only when, they appear in bold capitals. Byte values
are written in hexadecimal (`0x52`). `u8`, `u16` and `u32` are unsigned integers of 1, 2 and 4 bytes.

## 1. Transport

- All game communication uses **UDP**. Default server port: **4242**.
- One datagram contains exactly one packet. Maximum datagram size: **1200 bytes**.
- All integers are **unsigned, big-endian**. There is no floating-point field: a position is a `u16` count of 1/16 of a world unit (§4.2).

## 2. Packet header (10 bytes)

| Offset | Size | Field | Description |
|---|---|---|---|
| 0 | u16 | magic | Always `0x5254` ("RT") |
| 2 | u8 | version | Protocol version, currently `8` |
| 3 | u8 | type | Message type (see §4); bit `0x40` set means a reliable message (§3.2) |
| 4 | u32 | sequence | Number of the packet for this peer, see §3.1 |
| 8 | u16 | payload_size | Number of bytes after the header |

## 3. Validation

A receiver **silently drops** a datagram if any of these is true:

- it is shorter than 10 bytes or longer than 1200 bytes;
- `magic` or `version` doesn't match;
- `payload_size` differs from the actual number of bytes after the header;
- `type` is unknown, or the payload size is wrong for this type;
- an enumerated field has an unknown value.
- an `Input` packet sets any reserved flag bit (`0xC0`).

A malformed datagram **MUST NOT** crash the receiver or close the session.

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

### 3.2 Reliable messages

UDP may lose a datagram. A message that must arrive (a player left, an entity died, the client disconnects) is sent
as a **reliable** message; everything else (`Input`, `Snapshot`, `Ping`...) is sent without any guarantee, because
a newer one soon replaces it.

- **Sending.** The sender gives the message a **reliable number**: a `u32` counter per peer, starting at `0` and
  going up by `1` for each reliable message. It sets bit `0x40` of the `type` byte and puts the number in the first
  4 bytes of the payload, before the usual payload of the type (`payload_size` counts them). Example:
  `EntityDestroyed` (type `0x84`) is sent with `type = 0xC4` and a 9-byte payload (number, `net_id`, `reason`).
- **Acknowledging.** The receiver answers every reliable packet with an `Ack` (type `0x05`, payload: the `u32`
  reliable number). An `Ack` is never itself reliable, and a packet with `type = 0x45` is invalid.
- **Repeating.** Until the `Ack` arrives, the sender sends the same message again, **in a new packet** with the next
  sequence number. The wait before the first repeat is 200 ms, then it follows the measured round-trip time (the
  smoothed time between a first send and its `Ack`, plus a margin, between 50 ms and 1 s), and it doubles after
  each unanswered repeat (up to 8 times that, and never above 1 s). After 12 sends the sender gives up.
- **Receiving.** A receiver remembers the last 32 reliable numbers of each peer, as for sequence numbers (§3.1).
  A message whose number is new is handled; one it already handled is **not handled again**, but it is
  acknowledged again, because the previous `Ack` may be the datagram that was lost. Reliable messages are not
  ordered: two of them can be handled in the other order than they were sent.
- **Limit.** A sender has at most 32 reliable messages in flight, counted from the oldest one not yet
  acknowledged; the next ones wait. This is what lets the receiver tell a repeat from a new message.
- **New session.** `Connect` restarts the reliable numbers of both sides, like the sequence numbers (§3.1).

The messages sent reliably are `EntityDestroyed` and `PlayerLeft` (server to client) and `Disconnect` (client to
server). A server that receives a repeated reliable packet from an address it no longer knows (a client that was
removed after its `Disconnect`) still answers `Ack`, so that the sender stops repeating it.

### 3.3 Another protocol version

A datagram whose `version` differs from the receiver's cannot be decoded, and is dropped. One exception keeps a
client of another version from retrying forever: when a **server** drops a `Connect` (type `0x01`, which no
version has changed) for that reason, it answers `ConnectRejected` with reason `2`, written with **the version byte
of the client** so that the client can read it. The client then knows that the versions differ. A client of this
version that talks to an older server gets no answer and gives up after its connection attempts.

### 3.4 Rate limits

A server protects itself from a sender that floods it, with a token bucket (a steady rate plus a short burst):

- each connected client may send **300 datagrams per second**, with a burst of 150; a well-behaved client sends about 60
  `Input` per second, one `Ping` per second and an `Ack` per reliable message, so it never reaches this;
- all the senders the server does not know share a budget of **50 replies per second** (burst 25) for the answers it
  gives them: `ConnectRejected`, the answer to a `Connect` of another version, and the `Ack` of §3.2.

A datagram over the limit is dropped before anything is queued or answered, and counted. It does not close the
session: the sender is served again as soon as it slows down.

## 4. Messages

### 4.1 Client → server

| Type | Name | Payload | Description |
|---|---|---|---|
| `0x01` | Connect | — | Ask to join. Resent every 500 ms until ConnectAccepted or ConnectRejected. |
| `0x02` | Input | `u8 flags` | Keys currently held. Sent every client tick (60/s); also acts as keep-alive. |
| `0x03` | Disconnect | — | The client leaves. Sent reliably: the client waits up to 500 ms for the `Ack`. |
| `0x05` | Ack | `u32 reliable_number` | Confirms a reliable message. Sent by either side, never reliable (§3.2). |
| `0x04` | Ping | `u32 timestamp_ms` | Latency measurement; the server answers with Pong. The client sends one every second while connected, which also keeps the session alive. |

Input flags: `0x01` up, `0x02` down, `0x04` left, `0x08` right, `0x10` shoot, `0x20` ready.

### 4.2 Server → client

| Type | Name | Payload | Description |
|---|---|---|---|
| `0x81` | ConnectAccepted | `u8 player_id` | `player_id` in 0–3. |
| `0x82` | ConnectRejected | `u8 reason` | `1` server full, `2` version mismatch (see §3.3). |
| `0x83` | Snapshot | see below | Game state, score and entities at a server tick. |
| `0x84` | EntityDestroyed | `u32 net_id`, `u8 reason` | `1` killed, `2` out of bounds, `3` collected (bonus picked up, no explosion). Sent reliably. |
| `0x85` | PlayerLeft | `u8 player_id`, `u8 reason` | `1` disconnected, `2` timed out. Sent reliably. |
| `0x86` | Pong | `u32 timestamp_ms` | Same value as the Ping. |

#### Snapshot payload

| Size | Field |
|---|---|
| u32 | tick |
| u8 | state: `1` waiting for players, `2` playing, `3` finished (victory), `4` game over (defeat) |
| u32 | score |
| 4 × u8 | lives left of each player, by `player_id` (`0` if nobody has this id) |
| u8 | level: index of the level being played, from `0` |
| u8 | wave: waves of this level that have already come (`0` before the first one) |
| u16 | count (max 117) |
| count × 10 bytes | entities |

Each entity:

| Size | Field | Description |
|---|---|---|
| u32 | net_id | Network id of the entity, unique for the whole game |
| u8 | type | `1` player, `2` enemy, `3` player missile, `4` enemy missile, `5` obstacle, `6` bonus |
| u8 | variant | Player index (0–3) for players, subtype for other types |
| u16 | x | World position, quantized (see below) |
| u16 | y | |

**Position quantization.** A position `p` in world units is sent as `q = round((p + 1024) × 16)` and read back as
`p = q / 16 − 1024`. The world is 1920 × 1080 with its origin at the top-left; the range −1024 to +3071.94 leaves
room for entities that appear or leave outside the screen, and a value outside it is clamped to the nearest end (it
never wraps around). The error is at most 1/32 of a unit, far below one pixel, and an entity takes 10 bytes instead
of the 14 it took with two 32-bit floats. A sender **MUST** treat `NaN` and infinities as the ends of the range.

A tick with more than 117 entities is sent as several Snapshot packets with the same `tick`.

### 4.3 Snapshot rate

The server simulates 60 ticks per second but sends a snapshot only every **2 ticks** by default (30 per second;
`r-type_server --snapshot-every N` changes it). The `tick` of two consecutive snapshots therefore differs by the
sending interval, and a receiver **MUST NOT** expect consecutive tick numbers. Each snapshot is complete, so a lost one
costs nothing more than the wait for the next. A client **SHOULD** show the world slightly in the past and blend two
snapshots (the reference client shows it 2 sending intervals late), as described in [networking.md](networking.md)
§11. Losses are counted from the gaps in the packet numbers of §3.1, not from the tick numbers.

The end of the game is not a separate message: the client sees `state = 3` or `state = 4` in the snapshots, with the final score.
Since snapshots keep being sent (§4.3), this information cannot be lost.

## 5. Session

1. The client sends **Connect** until it receives **ConnectAccepted** or **ConnectRejected**.
2. The client sends **Input** every tick. The server sends **Snapshot** to every client every 2 ticks by default (30 per second).
3. The client keeps the entities of the newest tick it has received: the Snapshot packets of one tick are merged
   (duplicates are ignored), a Snapshot of a newer tick replaces them, and one of an older tick is dropped. An
   entity missing from the newest tick therefore disappears, even if its **EntityDestroyed** was lost. The client
   also removes it on **EntityDestroyed**. A new session (**ConnectAccepted**) resets the tick counter.
4. A client that sends nothing for **5 seconds** is considered gone: the server removes its ship and sends
   **PlayerLeft** (reason `2`) to the other clients. A **Disconnect** gives the same result with reason `1`.

## 6. Runtime conformance notes

The runtime implements every behavior described above. Only three messages are sent reliably (§3.2): `Connect` is
repeated by the client until it gets an answer, which is a separate mechanism. See the
[networking implementation guide](networking.md) for the current behavior and limitations.

## 7. Example datagrams

Both examples are produced byte for byte by the reference encoder (test `Protocol.SpecificationExamplesMatchTheEncoder`).

**Input** — packet number 7, the player holds *up* and *shoot* (`0x01 | 0x10`):

```text
52 54  08  02  00 00 00 07  00 01  |  11
magic  ver type sequence    size   |  flags
```

**Snapshot** — packet number 8, tick 120, playing, score 300, player 0 has 3 lives, second level (index 1),
second wave, one entity: the ship of player 0 (net id 5) at (100, 300). Quantized: x = (100 + 1024) × 16 = 17984 = `0x4640`, y = (300 + 1024) × 16 =
21184 = `0x52C0`.

```text
52 54 08 83 00 00 00 08 00 1B     header: Snapshot, sequence 8, payload of 27 bytes
00 00 00 78                       tick 120
02                                state: playing
00 00 01 2C                       score 300
03 00 00 00                       lives of players 0 to 3
01 02                             level index 1, wave 2
00 01                             one entity
00 00 00 05 01 00 46 40 52 C0     net id 5, player, variant 0, x, y
```

## 8. Security considerations

- Any host can send datagrams to the server port. Every field is validated (§3) and a datagram that fails is
  dropped without an answer, except `Connect` of another version (§3.3).
- A peer over its packet budget is ignored before anything is queued (§3.4), so a flood cannot exhaust memory.
- Clients send actions only; positions, damage and score are computed by the server, so a modified client cannot
  teleport or award itself points.
- There is no authentication or encryption: a host on the path can read the traffic or forge packets with the
  address of a player. The game carries no personal data; a session token is a possible extension.

