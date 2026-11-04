#include "common/Protocol.hpp"
#include "net/AsioNetworkClient.hpp"
#include "net/AsioNetworkServer.hpp"

#include <asio.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;
using rtype::client::AsioNetworkClient;
using rtype::client::ConnectionState;
using Udp = asio::ip::udp;

std::uint16_t findAvailablePort()
{
    asio::io_context io;
    Udp::socket probe(io, Udp::endpoint(Udp::v4(), 0));
    const auto port = probe.local_endpoint().port();
    probe.close();
    return port;
}

bool waitUntil(const std::function<bool()> &condition, std::chrono::milliseconds limit = 3000ms)
{
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (std::chrono::steady_clock::now() < deadline) {
        if (condition())
            return true;
        std::this_thread::sleep_for(5ms);
    }
    return condition();
}

// Short timings so the tests stay fast; the game uses the defaults.
AsioNetworkClient::Options fastOptions()
{
    AsioNetworkClient::Options options;
    options.connectRetryInterval = 40ms;
    options.maxConnectAttempts = 3;
    options.serverTimeout = 400ms;
    options.pingInterval = 50ms;
    return options;
}

rtype::protocol::Snapshot snapshotOfTick(std::uint32_t tick)
{
    return {tick, rtype::protocol::GameState::Playing, 0, {}};
}

} // namespace

TEST(NetworkClient, ConnectsMeasuresLatencyAndCountsTraffic)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t port = findAvailablePort();
    server.start(port);

    AsioNetworkClient client;
    client.setOptions(fastOptions());
    EXPECT_EQ(client.state(), ConnectionState::Idle);
    client.connect("127.0.0.1", port);

    ASSERT_TRUE(waitUntil([&] { return client.connected(); }));
    EXPECT_EQ(client.state(), ConnectionState::Connected);
    EXPECT_TRUE(waitUntil([&] { return client.stats().rttMs.has_value(); })) << "no Pong was received";
    EXPECT_LT(*client.stats().rttMs, 500.0);

    const auto clientTotals = client.stats().traffic.totals;
    EXPECT_GT(clientTotals.bytesSent, 0u);
    EXPECT_GT(clientTotals.bytesReceived, 0u);
    EXPECT_EQ(server.clientCount(), 1u);
    EXPECT_GT(server.traffic().totals.bytesReceived, 0u);
    EXPECT_GT(server.traffic().totals.bytesSent, 0u);

    client.disconnect();
    EXPECT_EQ(client.state(), ConnectionState::Idle);
    EXPECT_TRUE(waitUntil([&] { return server.clientCount() == 0u; }));
}

TEST(NetworkClient, EstimatesTheLossOfServerSnapshots)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t port = findAvailablePort();
    server.start(port);

    AsioNetworkClient client;
    client.setOptions(fastOptions());
    client.connect("127.0.0.1", port);
    ASSERT_TRUE(waitUntil([&] { return client.connected(); }));

    // Ticks 4 and 7 never get sent: 2 snapshots missing out of 8.
    for (std::uint32_t tick : {1u, 2u, 3u, 5u, 6u, 8u}) {
        server.broadcast(snapshotOfTick(tick));
        std::this_thread::sleep_for(2ms);
    }
    ASSERT_TRUE(waitUntil([&] { return client.stats().lossPercent > 0.0; }));
    EXPECT_DOUBLE_EQ(client.stats().lossPercent, 25.0);
}

TEST(NetworkClient, KeepsTheSessionAliveWithPingsWhenNothingElseIsSent)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t port = findAvailablePort();
    server.start(port);

    AsioNetworkClient client;
    client.setOptions(fastOptions()); // 400 ms server timeout
    client.connect("127.0.0.1", port);
    ASSERT_TRUE(waitUntil([&] { return client.connected(); }));

    std::this_thread::sleep_for(1200ms); // three times the timeout, with no Input and no snapshot
    EXPECT_EQ(client.state(), ConnectionState::Connected);
}

TEST(NetworkClient, GivesUpWhenNoServerAnswers)
{
    AsioNetworkClient client;
    client.setOptions(fastOptions());
    client.connect("127.0.0.1", findAvailablePort()); // nobody listens there

    EXPECT_EQ(client.state(), ConnectionState::Connecting);
    EXPECT_TRUE(waitUntil([&] { return client.state() == ConnectionState::TimedOut; }));
    EXPECT_FALSE(client.connected());
}

TEST(NetworkClient, ReportsARejectionWhenTheServerIsFull)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t port = findAvailablePort();
    server.start(port);

    asio::io_context io;
    std::vector<Udp::socket> players;
    for (std::size_t i = 0; i < rtype::MaxPlayers; ++i) {
        players.emplace_back(io, Udp::endpoint(Udp::v4(), 0));
        players.back().send_to(asio::buffer(rtype::protocol::encode({1, rtype::protocol::Connect{}})),
                               Udp::endpoint(asio::ip::address_v4::loopback(), port));
    }
    ASSERT_TRUE(waitUntil([&] { return server.clientCount() == rtype::MaxPlayers; }));

    AsioNetworkClient client;
    client.setOptions(fastOptions());
    client.connect("127.0.0.1", port);
    EXPECT_TRUE(waitUntil([&] { return client.state() == ConnectionState::Rejected; }));

    bool sawRejection = false;
    while (const auto message = client.poll())
        sawRejection |= std::holds_alternative<rtype::protocol::ConnectRejected>(*message);
    EXPECT_TRUE(sawRejection);
}

TEST(NetworkClient, ReportsLostWhenTheServerGoesSilent)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t port = findAvailablePort();
    server.start(port);

    AsioNetworkClient client;
    client.setOptions(fastOptions());
    client.connect("127.0.0.1", port);
    ASSERT_TRUE(waitUntil([&] { return client.connected(); }));

    server.stop();
    EXPECT_TRUE(waitUntil([&] { return client.state() == ConnectionState::Lost; }));
    EXPECT_FALSE(client.connected());
}

TEST(NetworkClient, CanConnectAgainAfterAFailure)
{
    const std::uint16_t port = findAvailablePort();
    AsioNetworkClient client;
    client.setOptions(fastOptions());
    client.connect("127.0.0.1", port);
    ASSERT_TRUE(waitUntil([&] { return client.state() == ConnectionState::TimedOut; }));
    client.disconnect();
    EXPECT_EQ(client.state(), ConnectionState::Idle);

    rtype::server::AsioNetworkServer server;
    server.start(port);
    client.connect("127.0.0.1", port);
    EXPECT_TRUE(waitUntil([&] { return client.connected(); }));
}
