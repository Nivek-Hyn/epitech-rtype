#pragma once

#include "engine/core/ThreadSafeQueue.hpp"
#include "net/INetworkClient.hpp"

#include <asio.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace rtype::client {

class AsioNetworkClient final : public INetworkClient {
public:
    AsioNetworkClient();
    ~AsioNetworkClient() override;

    void connect(const std::string &host, std::uint16_t port) override;
    void disconnect() override;
    bool connected() const override;
    std::optional<protocol::Message> poll() override;
    void send(const protocol::Message &message) override;

private:
    using Udp = asio::ip::udp;
    using Clock = std::chrono::steady_clock;

    void receive();
    void scheduleConnectRetry();
    void scheduleTimeoutCheck();
    void sendDatagram(protocol::Message message);

    asio::io_context _io;
    Udp::socket _socket;
    Udp::endpoint _server;
    Udp::endpoint _sender;
    asio::steady_timer _connectTimer;
    asio::steady_timer _timeoutTimer;
    std::array<std::uint8_t, protocol::MaxPacketSize> _receiveBuffer{};
    engine::core::ThreadSafeQueue<protocol::Message> _messages;
    std::atomic<bool> _connected{false};
    std::atomic<bool> _running{false};
    std::atomic<std::uint32_t> _nextSequence{0};
    Clock::time_point _lastReceive{};
    std::thread _thread;
};

} // namespace rtype::client