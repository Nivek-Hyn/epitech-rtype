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