#pragma once

#include "common/Constants.hpp"
#include "engine/core/ThreadSafeQueue.hpp"
#include "net/INetworkServer.hpp"

#include <asio.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace rtype::server {

class AsioNetworkServer final : public INetworkServer {
public:
    AsioNetworkServer();
    ~AsioNetworkServer() override;

    void start(std::uint16_t port) override;
    void stop() override;
    std::optional<NetworkEvent> poll() override;
    void send(PlayerId player, const protocol::Message &message) override;
    void broadcast(const protocol::Message &message) override;

private:
    using Udp = asio::ip::udp;
    using Clock = std::chrono::steady_clock;

    struct Client {
        bool active = false;
        Udp::endpoint endpoint;
        Clock::time_point lastSeen{};
    };

    void receive();
    void handleDatagram(std::size_t size);
    void scheduleTimeoutCheck();
    void sendTo(const Udp::endpoint &endpoint, protocol::Message message);
    void sendToPlayer(PlayerId player, protocol::Message message);
    std::optional<PlayerId> findClient(const Udp::endpoint &endpoint) const;
    void removeClient(PlayerId player, protocol::LeaveReason reason);

    asio::io_context _io;
    Udp::socket _socket;
    Udp::endpoint _sender;
    asio::steady_timer _timeoutTimer;
    std::array<std::uint8_t, protocol::MaxPacketSize> _receiveBuffer{};
    std::array<Client, MaxPlayers> _clients{};
    engine::core::ThreadSafeQueue<NetworkEvent> _events;
    std::atomic<std::uint32_t> _nextSequence{0};
    std::thread _thread;
};

} // namespace rtype::server