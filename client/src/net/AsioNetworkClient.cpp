#include "net/AsioNetworkClient.hpp"

#include "common/Constants.hpp"

#include <algorithm>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace rtype::client {

namespace {

constexpr double RttSmoothing = 0.2; // weight of the newest sample in the smoothed round-trip time

} // namespace

AsioNetworkClient::AsioNetworkClient() : _socket(_io), _connectTimer(_io), _housekeepingTimer(_io), _reliableTimer(_io) {}

AsioNetworkClient::~AsioNetworkClient()
{
    disconnect();
}

void AsioNetworkClient::connect(const std::string &host, std::uint16_t port)
{
    if (_thread.joinable())
        return;

    Udp::resolver resolver(_io);
    const auto endpoints = resolver.resolve(Udp::v4(), host, std::to_string(port));
    if (endpoints.empty())
        throw std::runtime_error("Could not resolve server host");
    _server = endpoints.begin()->endpoint();

    while (_messages.tryPop()) {
    } // nothing from a previous session

    _io.restart();
    _socket.open(Udp::v4());
    _traffic.reset();
    _rateMeter = engine::net::RateMeter{};
    _loss.reset();
    _incoming.reset();
    _reliable = engine::net::ReliableQueue<protocol::Message>{};
    _incomingReliable.reset();
    _closing = false;
    _sentRate = 0.0;
    _receivedRate = 0.0;
    _lossPercent = 0.0;
    _rttMs = -1.0;
    _connectAttempts = 0;
    _state = ConnectionState::Connecting;
    _running = true;
    _lastReceive = Clock::now();
    _lastPing = _lastReceive;
    receive();
    ++_connectAttempts;
    sendDatagram(protocol::Connect{});
    scheduleConnectRetry();
    scheduleHousekeeping();
    scheduleReliableTick();
    _thread = std::thread([this] { runIo(); });
}

// An exception escaping the I/O thread would terminate the whole client: report it and keep running.
void AsioNetworkClient::runIo()
{
    for (;;) {
        try {
            _io.run();
            return;
        } catch (const std::exception &error) {
            std::cerr << "r-type_client: network error: " << error.what() << '\n';
        } catch (...) {
            std::cerr << "r-type_client: unknown network error\n";
        }
    }
}

void AsioNetworkClient::disconnect()
{
    if (!_thread.joinable())
        return;

    asio::post(_io, [this] { beginClose(); });
    _thread.join();
    _connectTimer.cancel();
    _housekeepingTimer.cancel();
    _reliableTimer.cancel();
    asio::error_code error;
    _socket.close(error);
    _state = ConnectionState::Idle;
}

bool AsioNetworkClient::connected() const
{
    return _state == ConnectionState::Connected;
}

ConnectionState AsioNetworkClient::state() const
{
    return _state;
}

std::optional<protocol::Message> AsioNetworkClient::poll()
{
    return _messages.tryPop();
}

void AsioNetworkClient::sendReliable(const protocol::Message &message)
{
    if (!_running)
        return;
    asio::post(_io, [this, message] {
        if (!_running)
            return;
        _reliable.push(message);
        flushReliable(); // sends it right away; later copies come from the timer
    });
}

// Tells the server we leave. Connected: the Disconnect is reliable, so wait (a little) until the server confirms,
// otherwise it would only notice us gone after its timeout. Any other state: nobody to wait for.
void AsioNetworkClient::beginClose()
{
    if (!_running)
        return;
    if (_state != ConnectionState::Connected) {
        sendDatagram(protocol::Disconnect{});
        finishClose();
        return;
    }
    _closing = true;
    _closeDeadline = Clock::now() + _options.closeTimeout;
    _reliable.push(protocol::Disconnect{});
    flushReliable();
}

void AsioNetworkClient::finishClose()
{
    _running = false;
    _io.stop();
}

void AsioNetworkClient::send(const protocol::Message &message)
{
    if (!_running)
        return;
    asio::post(_io, [this, message] {
        if (_running)
            sendDatagram(message);
    });
}

NetworkStats AsioNetworkClient::stats() const
{
    NetworkStats result;
    result.traffic.totals = _traffic.totals();
    result.traffic.rates = {_sentRate, _receivedRate};
    const double rtt = _rttMs;
    if (rtt >= 0.0)
        result.rttMs = rtt;
    result.lossPercent = _lossPercent;
    return result;
}

void AsioNetworkClient::receive()
{
    _socket.async_receive_from(
        asio::buffer(_receiveBuffer), _sender, [this](const asio::error_code &error, std::size_t size) {
            if (!error && _sender == _server) {
                _traffic.addReceived(size);
                if (const auto packet = protocol::decode(std::span(_receiveBuffer).first(size)))
                    handlePacket(*packet, Clock::now());
            }
            if (_running)
                receive();
        });
}

void AsioNetworkClient::handlePacket(const protocol::Packet &packet, Clock::time_point now)
{
    _lastReceive = now; // even a copy proves that the server is alive

    // A copy of a packet already handled, or one far too old, is ignored.
    const auto verdict = _incoming.receive(packet.sequence);
    if (verdict == engine::net::SequenceVerdict::Duplicate) {
        _traffic.addDuplicate();
        return;
    }
    if (verdict == engine::net::SequenceVerdict::TooOld) {
        _traffic.addOutdated();
        return;
    }

    if (const auto *ack = std::get_if<protocol::Ack>(&packet.message)) {
        _reliable.acknowledge(ack->reliableId, now);
        if (_closing && _reliable.pending() == 0)
            finishClose();
        return; // internal to the transport: the game never sees it
    }
    if (packet.reliableId) {
        // Always answer, even to a copy: the previous Ack may be the datagram that was lost.
        sendDatagram(protocol::Ack{*packet.reliableId});
        const auto reliableVerdict = _incomingReliable.receive(*packet.reliableId);
        if (reliableVerdict == engine::net::SequenceVerdict::Duplicate ||
            reliableVerdict == engine::net::SequenceVerdict::TooOld) {
            _traffic.addDuplicate();
            return;
        }
    }

    // The server numbers the packets it sends to this client one by one, whatever they carry and however often it
    // sends snapshots, so a hole in the numbers is a packet that never arrived.
    _loss.record(packet.sequence);

    // Rejected, TimedOut and Lost are final: a late datagram must not bring a dead session back.
    const bool connecting = _state == ConnectionState::Connecting;

    if (std::holds_alternative<protocol::ConnectAccepted>(packet.message)) {
        if (connecting)
            _state = ConnectionState::Connected;
    } else if (std::holds_alternative<protocol::ConnectRejected>(packet.message)) {
        if (connecting)
            _state = ConnectionState::Rejected;
    } else if (std::holds_alternative<protocol::Snapshot>(packet.message)) {
        // A snapshot is the whole game at one tick: one that arrives after a newer one would only go back in time.
        if (verdict == engine::net::SequenceVerdict::Late) {
            _traffic.addOutdated();
            return;
        }
    } else if (const auto *pong = std::get_if<protocol::Pong>(&packet.message)) {
        const double sample = static_cast<double>(static_cast<std::uint32_t>(nowMs() - pong->timestampMs));
        const double previous = _rttMs;
        _rttMs = previous < 0.0 ? sample : previous + RttSmoothing * (sample - previous);
    }
    _messages.push(packet.message);
}

void AsioNetworkClient::scheduleConnectRetry()
{
    _connectTimer.expires_after(_options.connectRetryInterval);
    _connectTimer.async_wait([this](const asio::error_code &error) {
        if (error || !_running || _state != ConnectionState::Connecting)
            return;
        if (_connectAttempts >= _options.maxConnectAttempts) {
            _state = ConnectionState::TimedOut;
            return;
        }
        ++_connectAttempts;
        sendDatagram(protocol::Connect{});
        scheduleConnectRetry();
    });
}

void AsioNetworkClient::scheduleHousekeeping()
{
    // Often enough to notice a silent server quickly, but at most once per second in the game.
    const auto interval = std::clamp(std::min({std::chrono::milliseconds(1000), _options.serverTimeout / 4,
                                               _options.pingInterval}),
                                     std::chrono::milliseconds(5), std::chrono::milliseconds(1000));
    _housekeepingTimer.expires_after(interval);
    _housekeepingTimer.async_wait([this](const asio::error_code &error) {
        if (error || !_running)
            return;
        housekeep();
    });
}

// Once per tick of the housekeeping timer: bandwidth, loss, ping and silent-server detection.
void AsioNetworkClient::housekeep()
{
    const auto now = Clock::now();
    const auto rates = _rateMeter.sample(
        _traffic.totals(), std::chrono::duration<double>(now.time_since_epoch()).count());
    _sentRate = rates.sentBytesPerSecond;
    _receivedRate = rates.receivedBytesPerSecond;
    _lossPercent = _loss.lossPercent();

    if (_state == ConnectionState::Connected) {
        if (now - _lastReceive > _options.serverTimeout) {
            _state = ConnectionState::Lost;
            return;
        }
        if (now - _lastPing >= _options.pingInterval) {
            _lastPing = now;
            sendDatagram(protocol::Ping{nowMs()});
        }
    }
    scheduleHousekeeping();
}

void AsioNetworkClient::scheduleReliableTick()
{
    _reliableTimer.expires_after(std::chrono::milliseconds(50));
    _reliableTimer.async_wait([this](const asio::error_code &error) {
        if (error || !_running)
            return;
        flushReliable();
        if (_closing && (_reliable.pending() == 0 || Clock::now() >= _closeDeadline)) {
            finishClose();
            return;
        }
        scheduleReliableTick();
    });
}

void AsioNetworkClient::flushReliable()
{
    const auto retransmissions = _reliable.resendDue(
        Clock::now(), [this](std::uint32_t id, const protocol::Message &message) { sendDatagram(message, id); });
    _traffic.addRetransmissions(retransmissions);
}

void AsioNetworkClient::sendDatagram(protocol::Message message, std::optional<std::uint32_t> reliableId)
{
    auto bytes = std::make_shared<std::vector<std::uint8_t>>(
        protocol::encode({_nextSequence.fetch_add(1), std::move(message), reliableId}));
    _traffic.addSent(bytes->size());
    _socket.async_send_to(asio::buffer(*bytes), _server, [bytes](const asio::error_code &, std::size_t) {});
}

std::uint32_t AsioNetworkClient::nowMs() const
{
    return static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count());
}

} // namespace rtype::client
