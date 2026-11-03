#include "net/AsioNetworkServer.hpp"

#include "common/Constants.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <utility>

namespace rtype::server {

AsioNetworkServer::AsioNetworkServer() : _socket(_io), _timeoutTimer(_io) {}

AsioNetworkServer::~AsioNetworkServer()
{
    stop();
}

void AsioNetworkServer::start(std::uint16_t port)
{
    if (_thread.joinable())
        return;

    _io.restart();
    _socket.open(Udp::v4());
    _socket.bind(Udp::endpoint(Udp::v4(), port));
    receive();
    scheduleTimeoutCheck();
    _thread = std::thread([this] { runIo(); });
}

// An exception escaping the I/O thread would terminate the whole server: report it and keep serving.
void AsioNetworkServer::runIo()
{
    for (;;) {
        try {
            _io.run();
            return;
        } catch (const std::exception &error) {
            std::cerr << "r-type_server: network error: " << error.what() << '\n';
        } catch (...) {
            std::cerr << "r-type_server: unknown network error\n";
        }
    }
}

void AsioNetworkServer::stop()
{
    if (!_thread.joinable())
        return;

    _io.stop();
    _thread.join();
    _timeoutTimer.cancel();
    asio::error_code error;
    _socket.close(error);
}

std::optional<NetworkEvent> AsioNetworkServer::poll()
{
    return _events.tryPop();
}

void AsioNetworkServer::send(PlayerId player, const protocol::Message &message)
{
    asio::post(_io, [this, player, message] { sendToPlayer(player, message); });
}

void AsioNetworkServer::broadcast(const protocol::Message &message)
{
    asio::post(_io, [this, message] {
        for (PlayerId player = 0; player < MaxPlayers; ++player)
            sendToPlayer(player, message);
    });
}

void AsioNetworkServer::receive()
{
    _socket.async_receive_from(asio::buffer(_receiveBuffer), _sender,
                               [this](const asio::error_code &error, std::size_t size) {
                                   if (!error) {
                                       try {
                                           handleDatagram(size);
                                       } catch (const std::exception &failure) {
                                           std::cerr << "r-type_server: dropped a datagram: " << failure.what() << '\n';
                                       }
                                   }
                                   if (_socket.is_open())
                                       receive();
                               });
}

void AsioNetworkServer::handleDatagram(std::size_t size)
{
    const auto packet = protocol::decode(std::span(_receiveBuffer).first(size));
    if (!packet)
        return;

    const auto player = findClient(_sender);
    if (player)
        _clients[*player].lastSeen = Clock::now();

    if (std::holds_alternative<protocol::Connect>(packet->message)) {
        if (player) {
            sendToPlayer(*player, protocol::ConnectAccepted{*player});
            return;
        }
        for (PlayerId candidate = 0; candidate < MaxPlayers; ++candidate) {
            if (!_clients[candidate].active) {
                _clients[candidate] = Client{true, _sender, Clock::now()};
                _events.push(PlayerJoined{candidate});
                sendToPlayer(candidate, protocol::ConnectAccepted{candidate});
                return;
            }
        }
        sendTo(_sender, protocol::ConnectRejected{protocol::RejectReason::ServerFull});
        return;
    }

    if (!player)
        return;
    if (const auto *input = std::get_if<protocol::Input>(&packet->message)) {
        _events.push(PlayerInput{*player, input->flags});
    } else if (std::holds_alternative<protocol::Disconnect>(packet->message)) {
        removeClient(*player, protocol::LeaveReason::Disconnected);
    } else if (const auto *ping = std::get_if<protocol::Ping>(&packet->message)) {
        sendToPlayer(*player, protocol::Pong{ping->timestampMs});
    }
}

void AsioNetworkServer::scheduleTimeoutCheck()
{
    _timeoutTimer.expires_after(std::chrono::seconds(1));
    _timeoutTimer.async_wait([this](const asio::error_code &error) {
        if (error)
            return;
        const auto now = Clock::now();
        for (PlayerId player = 0; player < MaxPlayers; ++player) {
            if (_clients[player].active && now - _clients[player].lastSeen > rtype::ClientTimeout)
                removeClient(player, protocol::LeaveReason::TimedOut);
        }
        scheduleTimeoutCheck();
    });
}

void AsioNetworkServer::sendTo(const Udp::endpoint &endpoint, protocol::Message message)
{
    auto bytes =
        std::make_shared<std::vector<std::uint8_t>>(protocol::encode({_nextSequence.fetch_add(1), std::move(message)}));
    _socket.async_send_to(asio::buffer(*bytes), endpoint, [bytes](const asio::error_code &, std::size_t) {});
}

void AsioNetworkServer::sendToPlayer(PlayerId player, protocol::Message message)
{
    if (player >= MaxPlayers || !_clients[player].active)
        return;
    if (const auto *snapshot = std::get_if<protocol::Snapshot>(&message)) {
        for (auto &part : protocol::splitSnapshot(*snapshot))
            sendTo(_clients[player].endpoint, std::move(part));
        return;
    }
    sendTo(_clients[player].endpoint, std::move(message));
}

std::optional<PlayerId> AsioNetworkServer::findClient(const Udp::endpoint &endpoint) const
{
    for (PlayerId player = 0; player < MaxPlayers; ++player) {
        if (_clients[player].active && _clients[player].endpoint == endpoint)
            return player;
    }
    return std::nullopt;
}

void AsioNetworkServer::removeClient(PlayerId player, protocol::LeaveReason reason)
{
    if (player >= MaxPlayers || !_clients[player].active)
        return;
    _clients[player].active = false;
    _events.push(PlayerDisconnected{player, reason});
}

} // namespace rtype::server