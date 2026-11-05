#pragma once

#include "common/Protocol.hpp"
#include "engine/net/TrafficStats.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>

namespace rtype::server {

using PlayerId = std::uint8_t; // 0-3

// Events the network layer reports to the game thread.
struct PlayerJoined {
    PlayerId id;
};
struct PlayerDisconnected {
    PlayerId id;
    protocol::LeaveReason reason;
};
struct PlayerInput {
    PlayerId id;
    std::uint8_t flags;
};
using NetworkEvent = std::variant<PlayerJoined, PlayerDisconnected, PlayerInput>;

// Contract between the network layer and the rest of the server.
//
// The implementation runs its own thread (Asio) and handles by itself: the Connect handshake
// (ConnectAccepted / ConnectRejected), player id assignment, timeouts, Ping/Pong, and invalid packets.
// The game only sees the high-level events above.
class INetworkServer {
public:
    virtual ~INetworkServer() = default;

    virtual void start(std::uint16_t port) = 0;
    virtual void stop() = 0;

    // Called by the game thread every tick until it returns nullopt. Never blocks.
    virtual std::optional<NetworkEvent> poll() = 0;

    // Thread-safe. Send to one player or to every connected player.
    virtual void send(PlayerId player, const protocol::Message &message) = 0;
    virtual void broadcast(const protocol::Message &message) = 0;

    // Thread-safe. Same, for a message that must arrive (EntityDestroyed...): it is numbered, acknowledged by the
    // client and sent again until it is. Not for Snapshot, which is never split into reliable parts.
    virtual void sendReliable(PlayerId player, const protocol::Message &message) = 0;
    virtual void broadcastReliable(const protocol::Message &message) = 0;

    // Thread-safe. Datagrams and bytes exchanged with every client since start(), and the current bandwidth.
    virtual engine::net::TrafficStats traffic() const = 0;

    // Thread-safe. Number of connected clients.
    virtual std::size_t clientCount() const = 0;
};

} // namespace rtype::server
