#include "common/Protocol.hpp"
#include "net/AsioNetworkClient.hpp"
#include "net/AsioNetworkServer.hpp"

#include <asio.hpp>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
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

TEST(NetworkClient, SeesNoLossWhenTheServerSendsASnapshotEveryOtherTick)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t port = findAvailablePort();
    server.start(port);

    AsioNetworkClient client;
    client.setOptions(fastOptions());
    client.connect("127.0.0.1", port);
    ASSERT_TRUE(waitUntil([&] { return client.connected(); }));

    // The ticks of the snapshots are 2, 4, 6...: loss is judged on packet numbers, not on missing tick numbers.
    for (std::uint32_t tick = 2; tick <= 20; tick += 2) {
        server.broadcast(snapshotOfTick(tick));
        std::this_thread::sleep_for(2ms);
    }
    ASSERT_TRUE(waitUntil([&] { return client.stats().traffic.totals.packetsReceived >= 10; }));
    std::this_thread::sleep_for(150ms); // one more round of statistics
    EXPECT_DOUBLE_EQ(client.stats().lossPercent, 0.0);
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

TEST(NetworkClient, IgnoresDuplicatedAndStaleServerPackets)
{
    asio::io_context io;
    const std::uint16_t port = findAvailablePort();
    Udp::socket fakeServer(io, Udp::endpoint(Udp::v4(), port));
    fakeServer.non_blocking(true);

    AsioNetworkClient client;
    client.setOptions(fastOptions());
    client.connect("127.0.0.1", port);

    Udp::endpoint clientEndpoint;
    std::array<std::uint8_t, rtype::protocol::MaxPacketSize> buffer{};
    ASSERT_TRUE(waitUntil([&] {
        asio::error_code error;
        fakeServer.receive_from(asio::buffer(buffer), clientEndpoint, 0, error);
        return !error;
    })) << "the client never sent Connect";

    const auto sendNumbered = [&](std::uint32_t sequence, const rtype::protocol::Message &message) {
        fakeServer.send_to(asio::buffer(rtype::protocol::encode({sequence, message})), clientEndpoint);
    };
    sendNumbered(0, rtype::protocol::ConnectAccepted{0});
    sendNumbered(3, snapshotOfTick(5));
    sendNumbered(3, snapshotOfTick(5)); // a copy of the previous packet
    sendNumbered(2, snapshotOfTick(4)); // arrives after number 3: a snapshot from the past
    sendNumbered(1, rtype::protocol::EntityDestroyed{7, rtype::protocol::DestroyReason::Killed}); // late, still useful
    ASSERT_TRUE(waitUntil([&] { return client.stats().traffic.totals.packetsReceived >= 5; }));

    int accepted = 0;
    int snapshots = 0;
    int destroyed = 0;
    while (const auto message = client.poll()) {
        accepted += std::holds_alternative<rtype::protocol::ConnectAccepted>(*message);
        if (const auto *snapshot = std::get_if<rtype::protocol::Snapshot>(&*message)) {
            ++snapshots;
            EXPECT_EQ(snapshot->tick, 5u);
        }
        destroyed += std::holds_alternative<rtype::protocol::EntityDestroyed>(*message);
    }
    EXPECT_EQ(accepted, 1);
    EXPECT_EQ(snapshots, 1);
    EXPECT_EQ(destroyed, 1);
    EXPECT_EQ(client.stats().traffic.totals.duplicatesDropped, 1u);
    EXPECT_EQ(client.stats().traffic.totals.outdatedDropped, 1u);
}

namespace {

// A server we control: it answers the client's Connect with ConnectAccepted, then lets the test decide the rest.
struct FakeServer {
    asio::io_context io;
    std::uint16_t port = findAvailablePort();
    Udp::socket socket{io, Udp::endpoint(Udp::v4(), port)};
    Udp::endpoint client;
    std::uint32_t nextSequence = 0;

    FakeServer() { socket.non_blocking(true); }

    // Packets received within `limit` (or until `count` of them arrived), decoded.
    std::vector<rtype::protocol::Packet> receive(std::chrono::milliseconds limit, std::size_t count = 1000)
    {
        std::vector<rtype::protocol::Packet> packets;
        std::array<std::uint8_t, rtype::protocol::MaxPacketSize> buffer{};
        const auto deadline = std::chrono::steady_clock::now() + limit;
        while (std::chrono::steady_clock::now() < deadline && packets.size() < count) {
            asio::error_code error;
            const std::size_t size = socket.receive_from(asio::buffer(buffer), client, 0, error);
            if (error) {
                std::this_thread::sleep_for(2ms);
                continue;
            }
            if (const auto packet = rtype::protocol::decode(std::span(buffer).first(size)))
                packets.push_back(*packet);
        }
        return packets;
    }

    void send(const rtype::protocol::Message &message, std::optional<std::uint32_t> reliableId = std::nullopt)
    {
        socket.send_to(asio::buffer(rtype::protocol::encode({nextSequence++, message, reliableId})), client);
    }

    // Waits for the client's first datagram (its Connect), then accepts it.
    bool accept()
    {
        if (receive(1000ms, 1).empty())
            return false;
        send(rtype::protocol::ConnectAccepted{0});
        return true;
    }
};

int countAcks(const std::vector<rtype::protocol::Packet> &packets, std::uint32_t reliableId)
{
    int count = 0;
    for (const auto &packet : packets) {
        if (const auto *ack = std::get_if<rtype::protocol::Ack>(&packet.message))
            count += ack->reliableId == reliableId;
    }
    return count;
}

} // namespace

TEST(NetworkClient, AcknowledgesReliableMessagesAndHandsThemToTheGameOnce)
{
    FakeServer server;
    AsioNetworkClient client;
    client.setOptions(fastOptions());
    client.connect("127.0.0.1", server.port);
    ASSERT_TRUE(server.accept());

    const rtype::protocol::Message destroyed =
        rtype::protocol::EntityDestroyed{7, rtype::protocol::DestroyReason::Killed};
    server.send(destroyed, 4u);
    server.send(destroyed, 4u); // the server sent it again because it never saw our Ack

    const auto answers = server.receive(400ms);
    EXPECT_EQ(countAcks(answers, 4), 2) << "a copy must be acknowledged too: the first Ack may have been lost";

    int delivered = 0;
    while (const auto message = client.poll()) {
        if (const auto *event = std::get_if<rtype::protocol::EntityDestroyed>(&*message)) {
            ++delivered;
            EXPECT_EQ(event->netId, 7u);
        }
        EXPECT_FALSE(std::holds_alternative<rtype::protocol::Ack>(*message)) << "Ack is internal to the transport";
    }
    EXPECT_EQ(delivered, 1);
}

TEST(NetworkClient, DisconnectWaitsForTheServerToAcknowledgeIt)
{
    FakeServer server;
    AsioNetworkClient client;
    auto options = fastOptions();
    options.closeTimeout = 3000ms;
    client.setOptions(options);
    client.connect("127.0.0.1", server.port);
    ASSERT_TRUE(server.accept());
    ASSERT_TRUE(waitUntil([&] { return client.connected(); }));

    const auto started = std::chrono::steady_clock::now();
    std::thread closing([&] { client.disconnect(); });
    bool sawDisconnect = false;
    const auto deadline = started + 2s;
    while (!sawDisconnect && std::chrono::steady_clock::now() < deadline) {
        for (const auto &packet : server.receive(20ms)) {
            if (std::holds_alternative<rtype::protocol::Disconnect>(packet.message) && packet.reliableId) {
                sawDisconnect = true;
                server.send(rtype::protocol::Ack{*packet.reliableId});
            }
        }
    }
    closing.join();

    EXPECT_TRUE(sawDisconnect);
    EXPECT_LT(std::chrono::steady_clock::now() - started, 1500ms) << "it must not wait for the whole close timeout";
    EXPECT_EQ(client.state(), ConnectionState::Idle);
}

TEST(NetworkClient, DisconnectRepeatsItselfThenGivesUpWhenNobodyAcknowledges)
{
    FakeServer server;
    AsioNetworkClient client;
    auto options = fastOptions();
    options.closeTimeout = 450ms;
    client.setOptions(options);
    client.connect("127.0.0.1", server.port);
    ASSERT_TRUE(server.accept());
    ASSERT_TRUE(waitUntil([&] { return client.connected(); }));

    const auto started = std::chrono::steady_clock::now();
    std::thread closing([&] { client.disconnect(); });
    int disconnects = 0;
    while (std::chrono::steady_clock::now() < started + 800ms) {
        for (const auto &packet : server.receive(20ms))
            disconnects += std::holds_alternative<rtype::protocol::Disconnect>(packet.message) && packet.reliableId;
    }
    closing.join();

    EXPECT_GE(disconnects, 2) << "the Disconnect is sent again while no Ack comes";
    EXPECT_EQ(client.state(), ConnectionState::Idle);
}

TEST(NetworkClient, EstimatesTheLossFromGapsInThePacketNumbers)
{
    FakeServer server;
    AsioNetworkClient client;
    client.setOptions(fastOptions());
    client.connect("127.0.0.1", server.port);
    ASSERT_TRUE(server.accept()); // packet number 0

    // Numbers 1 and 4 are never sent: 2 packets missing out of the 8 numbered 0 to 7.
    for (std::uint32_t number : {2u, 3u, 5u, 6u, 7u}) {
        server.nextSequence = number;
        server.send(snapshotOfTick(number));
    }
    ASSERT_TRUE(waitUntil([&] { return client.stats().traffic.totals.packetsReceived >= 6; }));
    ASSERT_TRUE(waitUntil([&] { return client.stats().lossPercent > 0.0; }));
    EXPECT_DOUBLE_EQ(client.stats().lossPercent, 25.0);
}
