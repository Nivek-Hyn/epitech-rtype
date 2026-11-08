#pragma once

// The reference description is docs/protocol.md: keep both in sync, and bump Version on any breaking change.

#include "common/Constants.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace rtype::protocol {

inline constexpr std::uint16_t Magic = 0x5254; // "RT"
inline constexpr std::uint8_t Version = 8;
inline constexpr std::size_t HeaderSize = 10;
// Stay under the usual Internet MTU so datagrams are never fragmented.
inline constexpr std::size_t MaxPacketSize = 1200;

enum class MessageType : std::uint8_t {
    // client -> server
    Connect = 0x01,
    Input = 0x02,
    Disconnect = 0x03,
    Ping = 0x04,
    Ack = 0x05, // both directions: the receiver of a reliable message sends it back
    // server -> client
    ConnectAccepted = 0x81,
    ConnectRejected = 0x82,
    Snapshot = 0x83,
    EntityDestroyed = 0x84,
    PlayerLeft = 0x85,
    Pong = 0x86,
};

// Set in the type byte of a packet that must be delivered: its payload then starts with a u32 reliable number, and the
// receiver answers with an Ack carrying that number (the sender repeats the packet until it gets it).
inline constexpr std::uint8_t ReliableFlag = 0x40;

enum class EntityType : std::uint8_t {
    Player = 1,
    Enemy = 2,
    PlayerMissile = 3,
    EnemyMissile = 4,
    Obstacle = 5,
    Bonus = 6,
};

// Bit flags of the Input message.
namespace InputFlag {
inline constexpr std::uint8_t Up = 1 << 0;
inline constexpr std::uint8_t Down = 1 << 1;
inline constexpr std::uint8_t Left = 1 << 2;
inline constexpr std::uint8_t Right = 1 << 3;
inline constexpr std::uint8_t Shoot = 1 << 4;
inline constexpr std::uint8_t Ready = 1 << 5;
} // namespace InputFlag

enum class RejectReason : std::uint8_t { ServerFull = 1, VersionMismatch = 2 };
enum class DestroyReason : std::uint8_t { Killed = 1, OutOfBounds = 2, Collected = 3 };
enum class LeaveReason : std::uint8_t { Disconnected = 1, TimedOut = 2 };
enum class GameState : std::uint8_t { Waiting = 1, Playing = 2, Finished = 3, GameOver = 4 };

// ---- client -> server ----
struct Connect {};
struct Input {
    std::uint8_t flags = 0; // combination of InputFlag
};
struct Disconnect {};
struct Ping {
    std::uint32_t timestampMs = 0;
};

// Acknowledges the reliable message that had this number. Itself never reliable.
struct Ack {
    std::uint32_t reliableId = 0;
};

// ---- server -> client ----
struct ConnectAccepted {
    std::uint8_t playerId = 0; // 0-3
};
struct ConnectRejected {
    RejectReason reason = RejectReason::ServerFull;
};
struct EntityState {
    std::uint32_t netId = 0;
    EntityType type = EntityType::Player;
    std::uint8_t variant = 0;
    float x = 0.f;
    float y = 0.f;
};
// A position travels as a u16 counting 1/16 of a world unit, from -1024 to +3071.94: the world is 1920 x 1080 and
// entities appear or leave a few hundred units outside it. A value outside that range is clamped. The error is
// at most 1/32 of a unit, far below a pixel, and an entity costs 10 bytes instead of 14.
inline constexpr float PositionScale = 16.f;
inline constexpr float PositionOffset = 1024.f;
std::uint16_t quantizePosition(float value);
float dequantizePosition(std::uint16_t quantized);

inline constexpr std::size_t EntityStateSize = 10;
inline constexpr std::size_t SnapshotFixedSize = 17;
inline constexpr std::size_t MaxEntitiesPerSnapshot =
    (MaxPacketSize - HeaderSize - SnapshotFixedSize) / EntityStateSize;

// The whole game at a given server tick: game state, score and every entity.
// Sent every tick, so losing one is harmless. The end of the game is state == Finished (victory) or
// GameOver (defeat), with no separate message that could be lost. A tick with more than MaxEntitiesPerSnapshot entities
// is split into several Snapshot packets with the same tick.
struct Snapshot {
    std::uint32_t tick = 0;
    GameState state = GameState::Waiting;
    std::uint32_t score = 0;
    std::vector<EntityState> entities;
    std::array<std::uint8_t, MaxPlayers> lives{}; // lives left of each player, by player id
    std::uint8_t level = 0;                       // index of the level being played, from 0
    std::uint8_t wave = 0;                        // waves of this level that have already come
};
struct EntityDestroyed {
    std::uint32_t netId = 0;
    DestroyReason reason = DestroyReason::Killed;
};
struct PlayerLeft {
    std::uint8_t playerId = 0;
    LeaveReason reason = LeaveReason::Disconnected;
};
struct Pong {
    std::uint32_t timestampMs = 0;
};

using Message = std::variant<Connect, Input, Disconnect, Ping, Ack, ConnectAccepted, ConnectRejected, Snapshot,
                             EntityDestroyed, PlayerLeft, Pong>;

struct Packet {
    std::uint32_t sequence = 0; // incremented by the sender for each packet it sends to this peer
    Message message;
    std::optional<std::uint32_t> reliableId; // set for a message the receiver must acknowledge
};

// `version` is only meant for answering a peer that speaks another version (see otherVersionConnect).
std::vector<std::uint8_t> encode(const Packet &packet, std::uint8_t version = Version);

// A Connect written for another protocol version cannot be decoded, but its sender deserves an answer instead of
// silence. Returns that version when the datagram has our magic, a different version and the Connect type byte
// (which no version has changed), and nothing otherwise.
std::optional<std::uint8_t> otherVersionConnect(std::span<const std::uint8_t> datagram);

// Cuts a snapshot into one or more snapshots that each fit in a datagram (at most MaxEntitiesPerSnapshot entities).
// Every part keeps the tick, state and score; there is always at least one part, even without entities.
std::vector<Snapshot> splitSnapshot(const Snapshot &snapshot);

// Returns nullopt for any invalid datagram (bad magic/version, unknown type, wrong size...). Never throws.
std::optional<Packet> decode(std::span<const std::uint8_t> datagram);

} // namespace rtype::protocol
