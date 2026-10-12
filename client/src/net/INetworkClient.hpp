#pragma once

#include "common/Protocol.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace rtype::client {

// Contract between the client network layer and the rest of the client.
//
// The implementation runs its own thread (Asio): it sends Connect until ConnectAccepted/Rejected,
// numbers outgoing packets, drops invalid datagrams, and detects a silent server.
class INetworkClient {
public:
    virtual ~INetworkClient() = default;

    virtual void connect(const std::string &host, std::uint16_t port) = 0;
    virtual void disconnect() = 0;
    virtual bool connected() const = 0;

    // Called by the main thread every frame until it returns nullopt. Never blocks.
    virtual std::optional<protocol::Message> poll() = 0;

    // Thread-safe.
    virtual void send(const protocol::Message &message) = 0;
};

} // namespace rtype::client
