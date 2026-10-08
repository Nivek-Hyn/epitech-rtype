# R-Type Network Protocol — version 1 (draft)

This document describes every message exchanged between `r-type_client` and `r-type_server`.
It should be enough to write a new client. The reference implementation is `common/src/Protocol.cpp`.

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
| 4 | u32 | sequence | Incremented by the sender for each packet it sends |
| 8 | u16 | payload_size | Number of bytes after the header |

## 3. Validation

A receiver **silently drops** a datagram if any of these is true:

- it is shorter than 10 bytes or longer than 1200 bytes;
- `magic` or `version` doesn't match;
- `payload_size` differs from the actual number of bytes after the header;
- `type` is unknown, or the payload size is wrong for this type;
- an enumerated field has an unknown value.

A malformed datagram must never crash the receiver or close the session.

## 4. Messages

### 4.1 Client → server

| Type | Name | Payload | Description |
|---|---|---|---|
| `0x01` | Connect | — | Ask to join. Resent every 500 ms until ConnectAccepted or ConnectRejected. |
| `0x02` | Input | `u8 flags` | Keys currently held. Sent every client tick (60/s); also acts as keep-alive. |
| `0x03` | Disconnect | — | The client leaves. |
| `0x04` | Ping | `u32 timestamp_ms` | Latency measurement; the server answers with Pong. |

Input flags: `0x01` up, `0x02` down, `0x04` left, `0x08` right, `0x10` shoot.

### 4.2 Server → client

| Type | Name | Payload | Description |
|---|---|---|---|
| `0x81` | ConnectAccepted | `u8 player_id` | `player_id` in 0–3. |
| `0x82` | ConnectRejected | `u8 reason` | `1` server full, `2` version mismatch. |
| `0x83` | Snapshot | see below | State of the entities at a server tick. |
| `0x84` | EntityDestroyed | `u32 net_id`, `u8 reason` | `1` killed, `2` out of bounds. |
| `0x85` | PlayerLeft | `u8 player_id`, `u8 reason` | `1` disconnected, `2` timed out. |
| `0x86` | GameOver | `u32 score` | End of the game. |
| `0x87` | Pong | `u32 timestamp_ms` | Same value as the Ping. |

#### Snapshot payload

| Size | Field |
|---|---|
| u32 | tick |
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

## 5. Session

1. The client sends **Connect** until it receives **ConnectAccepted** or **ConnectRejected**.
2. The client sends **Input** every tick. The server sends **Snapshot** every tick to every client.
3. The client creates an entity the first time its `net_id` appears in a snapshot. It removes it on
   **EntityDestroyed**, or when it hasn't appeared in any snapshot for 500 ms.
4. A client that sends nothing for **5 seconds** is considered gone: the server removes its ship and sends
   **PlayerLeft** (reason `2`) to the other clients. A **Disconnect** gives the same result with reason `1`.
