#include "common/Protocol.hpp"
#include "net/AsioNetworkServer.hpp"

#include <asio.hpp>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <thread>
#include <vector>

namespace {

using Udp = asio::ip::udp;

std::uint16_t findAvailablePort()
{
    asio::io_context io;
    Udp::socket probe(io, Udp::endpoint(Udp::v4(), 0));
    const auto port = probe.local_endpoint().port();
    probe.close();
    return port;
}

std::optional<rtype::protocol::Packet> receivePacket(Udp::socket &socket, Udp::endpoint &sender)
{
    std::array<std::uint8_t, rtype::protocol::MaxPacketSize> buffer{};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);

    while (std::chrono::steady_clock::now() < deadline) {
        asio::error_code error;
        const std::size_t size = socket.receive_from(asio::buffer(buffer), sender, 0, error);
        if (!error)
            return rtype::protocol::decode(std::span(buffer).first(size));
        if (error != asio::error::would_block && error != asio::error::try_again)
            return std::nullopt;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return std::nullopt;
}

} // namespace

TEST(NetworkIntegration, AcceptsFourUdpClientsAndRejectsTheFifth)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t serverPort = findAvailablePort();
    ASSERT_NO_THROW(server.start(serverPort));

    asio::io_context io;
    std::vector<Udp::socket> clients;
    clients.reserve(rtype::MaxPlayers + 1);
    for (std::size_t i = 0; i < rtype::MaxPlayers + 1; ++i) {
        clients.emplace_back(io, Udp::endpoint(Udp::v4(), 0));
        clients.back().non_blocking(true);
    }

    for (std::size_t i = 0; i < clients.size(); ++i) {
        const auto request = rtype::protocol::encode(
            {static_cast<std::uint32_t>(i + 1), rtype::protocol::Connect{}});
        clients[i].send_to(asio::buffer(request),
                           Udp::endpoint(asio::ip::address_v4::loopback(), serverPort));

        Udp::endpoint sender;
        const auto response = receivePacket(clients[i], sender);
        ASSERT_TRUE(response) << "No response received for client " << i;
        ASSERT_EQ(sender, Udp::endpoint(asio::ip::address_v4::loopback(), serverPort));

        if (i < rtype::MaxPlayers) {
            ASSERT_TRUE(std::holds_alternative<rtype::protocol::ConnectAccepted>(response->message));
            EXPECT_EQ(std::get<rtype::protocol::ConnectAccepted>(response->message).playerId, i);
        } else {
            ASSERT_TRUE(std::holds_alternative<rtype::protocol::ConnectRejected>(response->message));
            EXPECT_EQ(std::get<rtype::protocol::ConnectRejected>(response->message).reason,
                      rtype::protocol::RejectReason::ServerFull);
        }
    }
}
TEST(NetworkIntegration, SplitsSnapshotsLargerThanOneDatagramAndStaysAlive)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t serverPort = findAvailablePort();
    ASSERT_NO_THROW(server.start(serverPort));

    asio::io_context io;
    Udp::socket client(io, Udp::endpoint(Udp::v4(), 0));
    client.non_blocking(true);
    const Udp::endpoint serverEndpoint(asio::ip::address_v4::loopback(), serverPort);

    client.send_to(asio::buffer(rtype::protocol::encode({1, rtype::protocol::Connect{}})), serverEndpoint);
    Udp::endpoint sender;
    const auto accepted = receivePacket(client, sender);
    ASSERT_TRUE(accepted);
    ASSERT_TRUE(std::holds_alternative<rtype::protocol::ConnectAccepted>(accepted->message));

    constexpr std::size_t EntityCount = rtype::protocol::MaxEntitiesPerSnapshot * 2 + 10;
    rtype::protocol::Snapshot big{12, rtype::protocol::GameState::Playing, 0, {}};
    for (std::size_t i = 0; i < EntityCount; ++i)
        big.entities.push_back({static_cast<std::uint32_t>(i + 1), rtype::protocol::EntityType::EnemyMissile, 0, 0.f, 0.f});
    server.broadcast(big);

    std::size_t received = 0;
    for (int packets = 0; packets < 3; ++packets) {
        const auto packet = receivePacket(client, sender);
        ASSERT_TRUE(packet) << "Missing snapshot part " << packets;
        const auto *snapshot = std::get_if<rtype::protocol::Snapshot>(&packet->message);
        ASSERT_NE(snapshot, nullptr);
        EXPECT_EQ(snapshot->tick, 12u);
        received += snapshot->entities.size();
    }
    EXPECT_EQ(received, EntityCount);

    // The server must still answer after having sent a snapshot that used to make it crash.
    client.send_to(asio::buffer(rtype::protocol::encode({2, rtype::protocol::Ping{77}})), serverEndpoint);
    const auto pong = receivePacket(client, sender);
    ASSERT_TRUE(pong);
    ASSERT_TRUE(std::holds_alternative<rtype::protocol::Pong>(pong->message));
}
