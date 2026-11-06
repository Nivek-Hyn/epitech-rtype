#pragma once

// Binary protocol v2 between r-type_client and r-type_server, over UDP.
// The reference description is docs/protocol.md: keep both in sync, and bump Version on any breaking change.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace rtype::protocol {

inline constexpr std::uint16_t Magic = 0x5254; // "RT"
inline constexpr std::uint8_t Version = 2;
inline constexpr std::size_t HeaderSize = 10;
// Stay under the usual Internet MTU so datagrams are never fragmented.
inline constexpr std::size_t MaxPacketSize = 1200;

enum class MessageType : std::uint8_t {
    // client -> server
    Connect = 0x01,
    Input = 0x02,
    Disconnect = 0x03,
    Ping = 0x04,
    // server -> client
    ConnectAccepted = 0x81,
    ConnectRejected = 0x82,
    Snapshot = 0x83,
    EntityDestroyed = 0x84,
    PlayerLeft = 0x85,
    Pong = 0x86,
};

enum class EntityType : std::uint8_t {
    Player = 1,
    Enemy = 2,
    PlayerMissile = 3,
    EnemyMissile = 4,
};

// Bit flags of the Input message.
namespace InputFlag {
inline constexpr std::uint8_t Up = 1 << 0;
inline constexpr std::uint8_t Down = 1 << 1;
inline constexpr std::uint8_t Left = 1 << 2;
inline constexpr std::uint8_t Right = 1 << 3;
inline constexpr std::uint8_t Shoot = 1 << 4;
} // namespace InputFlag

enum class RejectReason : std::uint8_t { ServerFull = 1, VersionMismatch = 2 };
enum class DestroyReason : std::uint8_t { Killed = 1, OutOfBounds = 2 };
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
inline constexpr std::size_t EntityStateSize = 14;
inline constexpr std::size_t SnapshotFixedSize = 11;
inline constexpr std::size_t MaxEntitiesPerSnapshot =
    (MaxPacketSize - HeaderSize - SnapshotFixedSize) / EntityStateSize;

// The whole game at a given server tick: game state, score and every entity.
// Sent every tick, so losing one is harmless. The end of the game is state == Finished (victory) or
// GameOver (defeat), with no separate message that could be lost. A tick with more than MaxEntitiesPerSnapshot entities is split into several
// Snapshot packets with the same tick.
struct Snapshot {
    std::uint32_t tick = 0;
    GameState state = GameState::Waiting;
    std::uint32_t score = 0;
    std::vector<EntityState> entities;
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

using Message = std::variant<Connect, Input, Disconnect, Ping, ConnectAccepted, ConnectRejected, Snapshot,
                             EntityDestroyed, PlayerLeft, Pong>;

struct Packet {
    std::uint32_t sequence = 0; // incremented by the sender for each packet
    Message message;
};

std::vector<std::uint8_t> encode(const Packet &packet);

// Returns nullopt for any invalid datagram (bad magic/version, unknown type, wrong size...). Never throws.
std::optional<Packet> decode(std::span<const std::uint8_t> datagram);

} // namespace rtype::protocol
