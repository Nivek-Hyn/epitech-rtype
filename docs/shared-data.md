# Shared data and ownership

This page describes the current code in `common/`, `Game` and the client, as included in the local
combined work. It is a reference for issues #23 and #101, not evidence that future interface changes
have been agreed by all four team members. See the [validation record](validation.md) for tested revisions.

## Three different identifiers

| Identifier | Meaning | Lifetime and use |
|---|---|---|
| `engine::ecs::Entity` | Local Registry slot, represented by `std::uint32_t` | Recycled after kill. Never send this local id over the network. |
| `NetworkId::id` | Server-assigned identity for a networked game object | `Game::spawnNetworked` assigns the next counter value, starting at 1. Used by snapshots and destruction messages. Counter overflow is not handled explicitly. |
| `PlayerId` | One of the server's four connection slots, 0–3 | Used for inputs and player connection events; not an entity id or NetworkId. |

`NullEntity` is the largest local Entity value and represents an absent local entity. A network id and a
local Entity can happen to have the same numeric value, but their meaning is different.
`Game::_players` maps player slots to local entities; the NetworkId component maps an entity to its wire identity.

## Components present today

`common/include/common/Components.hpp` defines these data-only structs:

| Component | Fields | Meaning | Sent in an entity snapshot? |
|---|---|---|---|
| Position | `float x`, `float y` | World coordinates | Yes, copied into EntityState. |
| Velocity | `float x`, `float y` | World units per second | No; the server uses it for movement. |
| NetworkId | `std::uint32_t id` | Network object identity | Yes, as `netId`. |
| Kind | `EntityType type`, `std::uint8_t variant` | Broad object category and game-defined subtype | Yes. |
| Hitbox | `float width`, `float height`, `float offsetY` | Area that can be hit, relative to Position | No. |
| Health, MovementPattern, EnemyAttack, EnemyTraits, MissileDamage, BonusEffect, PlayerControl | see the [how-to guides](how-to.md#add-an-enemy-a-tile-or-a-bonus) | Gameplay data read from prefabs | No. |

The origin is top-left: x grows rightwards and y downwards. The world is 1920×1080 units; the client scales
it to its window. A simulation update receives seconds, so movement adds velocity multiplied by that duration.
Window pixels and world units are not interchangeable without that view conversion.

Shared components belong in `common/`; game-independent operations remain in `engine/`. Data used only by
server gameplay belongs in `server/`, while textures, UI and SFML objects belong in `client/`.
Every component above can appear in a prefab of `assets/config/entities.json`. Collisions build AABBs from
the Position and Hitbox of each entity (GameCollisions.cpp). A player killed by a collision loses its local player/entity association before
the id is recycled; a dead player's inputs are ignored.

## Input and state flow

1. The client maps keys to `InputFlag` actions: up, down, left, right and shoot.
2. The network layer decodes valid Input packets and assigns them to a player slot.
3. Game changes that player's velocity and applies the server-side shooting cooldown.
4. The fixed-step update advances configured waves, moves entities and applies collisions.
5. Game builds a snapshot from Position, Kind and NetworkId; the client displays received states.

Remapping a key changes the client mapping, not the meaning of an Input flag. The current client does not
maintain a local Registry or interpolate remote states. Velocity is not implicitly transferred by including
its C++ struct in common/.

The server loads its initial wave schedule from `assets/levels/level1.json`. Level files configure timing,
positions and speed; they do not change the wire format or provide campaign transitions by themselves.

## C++ storage and wire data

`common/Protocol.hpp` defines Packet, Snapshot and EntityState; Protocol.cpp encodes their fields explicitly.
The [protocol specification](protocol.md) gives message values and byte layout.

- One EntityState is encoded as 14 bytes: network id, type, variant, x and y.
- That wire size is not a promise about `sizeof(EntityState)`: C++ padding and alignment are separate.
- Numeric fields are written in network byte order. Floats are converted to their 32-bit representation.
- Snapshot carries tick, game state, score, the lives of each player and the entity list. It does not carry health, damage animations,
  arbitrary component data or level names.
- Game sets snapshot state to Playing, then Finished after the last campaign level or GameOver when every
  connected player is out of lives. Waiting exists in the enum but is not used yet.

The current packet limit permits 83 entities in one snapshot. The encoder rejects a larger snapshot;
the current server/client do not provide complete snapshot fragmentation and reassembly. Header comments
describing splitting are not proof that it exists. Coordinate the network change through #105 before
assuming multiple packets with the same tick form a complete state.

Destruction messages refer to NetworkId, not a local slot. Individual UDP events are not made reliable by
sending snapshots periodically; sequence handling, retransmission and interpolation remain separate work.

## Changing the shared contract

Before adding a shared field, agree across gameplay, networking and rendering:

| Question | Required decision |
|---|---|
| Is the data durable state or a temporary effect? | Define what a late or reconnecting client must recover. |
| Who calculates it? | Keep gameplay authority on the server; define client display/input responsibilities. |
| Does it need to cross the network? | A new component alone does not alter EntityState or serialization. |
| What are its bounds and units? | Specify representation, valid ranges and invalid-packet behavior. |
| Is it compatible with protocol v1? | Update version and documentation together when required. |
| How is loss/reordering handled? | Define reliability, sequence and snapshot-completion rules explicitly. |
| How is it verified? | Add round-trip and malformed-message tests, plus the gameplay/client usage check. |

Issues #101, #105, #106, #116 and the gameplay work must resolve those decisions with their owners.
This page does not choose new health fields, introduce a second protocol version or assign another member's work.
