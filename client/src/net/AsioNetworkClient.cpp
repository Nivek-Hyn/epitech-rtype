#include "net/AsioNetworkClient.hpp"

#include "common/Constants.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

namespace rtype::client {

AsioNetworkClient::AsioNetworkClient() : _socket(_io), _connectTimer(_io), _timeoutTimer(_io) {}

AsioNetworkClient::~AsioNetworkClient() { disconnect(); }

void AsioNetworkClient::connect(const std::string &host, std::uint16_t port)
{
    if (_thread.joinable())
        return;

    Udp::resolver resolver(_io);
    const auto endpoints = resolver.resolve(Udp::v4(), host, std::to_string(port));
    if (endpoints.empty())
        throw std::runtime_error("Could not resolve server host");
    _server = endpoints.begin()->endpoint();

    _io.restart();
    _socket.open(Udp::v4());
    _connected = false;
    _connectionRejected = false;
    _running = true;
    _lastReceive = Clock::now();
    receive();
    sendDatagram(protocol::Connect{});
    scheduleConnectRetry();
    scheduleTimeoutCheck();
    _thread = std::thread([this] { _io.run(); });
}

void AsioNetworkClient::disconnect()
{
    if (!_thread.joinable())
        return;

    asio::post(_io, [this] {
        if (_running) {
            const auto bytes = protocol::encode({_nextSequence.fetch_add(1), protocol::Disconnect{}});
            asio::error_code ignored;
            _socket.send_to(asio::buffer(bytes), _server, 0, ignored);
        }
        _running = false;
        _connected = false;
        _io.stop();
    });
    _thread.join();
    _connectTimer.cancel();
    _timeoutTimer.cancel();
    asio::error_code error;
    _socket.close(error);
}

bool AsioNetworkClient::connected() const { return _connected; }

std::optional<protocol::Message> AsioNetworkClient::poll() { return _messages.tryPop(); }

void AsioNetworkClient::send(const protocol::Message &message)
{
    if (!_running)
        return;
    asio::post(_io, [this, message] {
        if (_running)
            sendDatagram(message);
    });
}

void AsioNetworkClient::receive()
{
    _socket.async_receive_from(asio::buffer(_receiveBuffer), _sender,
                               [this](const asio::error_code &error, std::size_t size) {
                                   if (!error && _sender == _server) {
                                       if (const auto packet = protocol::decode(std::span(_receiveBuffer).first(size))) {
                                           _lastReceive = Clock::now();
                                           if (std::holds_alternative<protocol::ConnectAccepted>(packet->message)) {
                                               _connected = true;
                                               _connectionRejected = false;
                                           } else if (std::holds_alternative<protocol::ConnectRejected>(packet->message)) {
                                               _connected = false;
                                               _connectionRejected = true;
                                           }
                                           _messages.push(packet->message);
                                       }
                                   }
                                   if (_running)
                                       receive();
                               });
}

void AsioNetworkClient::scheduleConnectRetry()
{
    _connectTimer.expires_after(std::chrono::milliseconds(500));
    _connectTimer.async_wait([this](const asio::error_code &error) {
        if (error || !_running || _connectionRejected)
            return;
        if (!_connected)
            sendDatagram(protocol::Connect{});
        scheduleConnectRetry();
    });
}

void AsioNetworkClient::scheduleTimeoutCheck()
{
    _timeoutTimer.expires_after(std::chrono::seconds(1));
    _timeoutTimer.async_wait([this](const asio::error_code &error) {
        if (error || !_running)
            return;
        if (Clock::now() - _lastReceive > rtype::ClientTimeout)
            _connected = false;
        scheduleTimeoutCheck();
    });
}

void AsioNetworkClient::sendDatagram(protocol::Message message)
{
    auto bytes = std::make_shared<std::vector<std::uint8_t>>(
        protocol::encode({_nextSequence.fetch_add(1), std::move(message)}));
    _socket.async_send_to(asio::buffer(*bytes), _server,
                          [bytes](const asio::error_code &, std::size_t) {});
}

} // namespace rtype::client