#pragma once

#include "common/Constants.hpp"
#include "engine/core/ThreadSafeQueue.hpp"
#include "engine/net/TrafficStats.hpp"
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
    engine::net::TrafficStats traffic() const override;
    std::size_t clientCount() const override { return _clientCount; }

private:
    using Udp = asio::ip::udp;
    using Clock = std::chrono::steady_clock;

    struct Client {
        bool active = false;
        Udp::endpoint endpoint;
        Clock::time_point lastSeen{};
    };

    void runIo();
    void receive();
    void handleDatagram(std::size_t size);
    void scheduleTimeoutCheck();
    void updateRates();
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
    engine::net::TrafficCounter _traffic;
    engine::net::RateMeter _rateMeter; // I/O thread only
    std::atomic<double> _sentRate{0.0};
    std::atomic<double> _receivedRate{0.0};
    std::atomic<std::size_t> _clientCount{0};
    std::thread _thread;
};

} // namespace rtype::server