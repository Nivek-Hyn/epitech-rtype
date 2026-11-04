#pragma once

#include "common/Constants.hpp"
#include "engine/core/ThreadSafeQueue.hpp"
#include "engine/net/LossTracker.hpp"
#include "engine/net/TrafficStats.hpp"
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
    /// Timings of the connection; the defaults are the game's. Tests use shorter ones.
    struct Options {
        std::chrono::milliseconds connectRetryInterval = rtype::ConnectRetryInterval;
        int maxConnectAttempts = rtype::MaxConnectAttempts;
        std::chrono::milliseconds serverTimeout = std::chrono::duration_cast<std::chrono::milliseconds>(rtype::ClientTimeout);
        std::chrono::milliseconds pingInterval = std::chrono::duration_cast<std::chrono::milliseconds>(rtype::PingInterval);
    };

    AsioNetworkClient();
    ~AsioNetworkClient() override;

    /// Must be called before connect().
    void setOptions(const Options &options) { _options = options; }

    void connect(const std::string &host, std::uint16_t port) override;
    void disconnect() override;
    bool connected() const override;
    ConnectionState state() const override;
    std::optional<protocol::Message> poll() override;
    void send(const protocol::Message &message) override;
    NetworkStats stats() const override;

private:
    using Udp = asio::ip::udp;
    using Clock = std::chrono::steady_clock;

    void runIo();
    void receive();
    void handlePacket(const protocol::Packet &packet, Clock::time_point now);
    void scheduleConnectRetry();
    void scheduleHousekeeping();
    void housekeep();
    void sendDatagram(protocol::Message message);
    std::uint32_t nowMs() const;

    asio::io_context _io;
    Udp::socket _socket;
    Udp::endpoint _server;
    Udp::endpoint _sender;
    asio::steady_timer _connectTimer;
    asio::steady_timer _housekeepingTimer;
    std::array<std::uint8_t, protocol::MaxPacketSize> _receiveBuffer{};
    engine::core::ThreadSafeQueue<protocol::Message> _messages;
    Options _options;
    std::atomic<ConnectionState> _state{ConnectionState::Idle};
    std::atomic<bool> _running{false};
    std::atomic<std::uint32_t> _nextSequence{0};
    Clock::time_point _lastReceive{};
    Clock::time_point _lastPing{};
    int _connectAttempts = 0; // I/O thread only

    engine::net::TrafficCounter _traffic;
    engine::net::RateMeter _rateMeter;  // I/O thread only
    engine::net::LossTracker _loss;     // I/O thread only
    std::atomic<double> _sentRate{0.0};
    std::atomic<double> _receivedRate{0.0};
    std::atomic<double> _lossPercent{0.0};
    std::atomic<double> _rttMs{-1.0}; // negative until the first Pong
    std::thread _thread;
};

} // namespace rtype::client
