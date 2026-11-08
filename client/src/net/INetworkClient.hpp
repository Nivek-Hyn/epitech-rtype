#pragma once

#include "common/Protocol.hpp"
#include "engine/net/TrafficStats.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace rtype::client {

/// Where the connection to the server stands.
enum class ConnectionState : std::uint8_t {
    Idle,       ///< connect() was not called, or disconnect() was.
    Connecting, ///< Connect was sent, no answer yet.
    Connected,  ///< The server accepted us and keeps answering.
    Rejected,   ///< The server answered ConnectRejected (the reason is in that message).
    TimedOut,   ///< The server never answered the Connect attempts.
    Lost,       ///< The server was connected but went silent.
};

/// Quality of the link, measured by the client.
struct NetworkStats {
    engine::net::TrafficStats traffic;
    std::optional<double> rttMs; ///< Smoothed round-trip time; empty until a Pong was received.
    double lossPercent =
        0.0; ///< Share of the packets from the server that never reached us, from gaps in their numbers.
};

// Contract between the client network layer and the rest of the client.
//
// The implementation runs its own thread (Asio): it sends Connect until ConnectAccepted/Rejected (giving up after
// a while), numbers outgoing packets, drops invalid datagrams, pings the server, and detects a silent server.
class INetworkClient {
public:
    virtual ~INetworkClient() = default;

    virtual void connect(const std::string &host, std::uint16_t port) = 0;
    virtual void disconnect() = 0;
    virtual bool connected() const = 0;

    // Never blocks. A final state (Rejected, TimedOut, Lost) stays until disconnect() or the next connect().
    virtual ConnectionState state() const = 0;

    // Called by the main thread every frame until it returns nullopt. Never blocks.
    virtual std::optional<protocol::Message> poll() = 0;

    // Thread-safe.
    virtual void send(const protocol::Message &message) = 0;

    // Thread-safe. For a message that must arrive: it is numbered, acknowledged by the server and sent again until
    // it is. disconnect() uses it for Disconnect and waits (briefly) for the acknowledgement.
    virtual void sendReliable(const protocol::Message &message) = 0;

    // Thread-safe. Bandwidth, latency and loss since connect().
    virtual NetworkStats stats() const = 0;
};

} // namespace rtype::client
