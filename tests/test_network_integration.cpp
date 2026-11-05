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

TEST(NetworkIntegration, NotifiesRemainingClientsWhenAPlayerDisconnects)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t serverPort = findAvailablePort();
    ASSERT_NO_THROW(server.start(serverPort));

    asio::io_context io;
    const Udp::endpoint serverEndpoint(asio::ip::address_v4::loopback(), serverPort);
    std::vector<Udp::socket> clients;
    for (int i = 0; i < 2; ++i) {
        clients.emplace_back(io, Udp::endpoint(Udp::v4(), 0));
        clients.back().non_blocking(true);
        clients.back().send_to(asio::buffer(rtype::protocol::encode({1, rtype::protocol::Connect{}})), serverEndpoint);
        Udp::endpoint sender;
        const auto accepted = receivePacket(clients.back(), sender);
        ASSERT_TRUE(accepted);
        ASSERT_TRUE(std::holds_alternative<rtype::protocol::ConnectAccepted>(accepted->message));
    }

    clients[0].send_to(asio::buffer(rtype::protocol::encode({2, rtype::protocol::Disconnect{}})), serverEndpoint);

    Udp::endpoint sender;
    const auto notice = receivePacket(clients[1], sender);
    ASSERT_TRUE(notice);
    const auto *left = std::get_if<rtype::protocol::PlayerLeft>(&notice->message);
    ASSERT_NE(left, nullptr);
    EXPECT_EQ(left->playerId, 0);
    EXPECT_EQ(left->reason, rtype::protocol::LeaveReason::Disconnected);
}

namespace {

using rtype::protocol::Message;

void sendFrom(Udp::socket &socket, std::uint16_t serverPort, std::uint32_t sequence, const Message &message)
{
    socket.send_to(asio::buffer(rtype::protocol::encode({sequence, message})),
                   Udp::endpoint(asio::ip::address_v4::loopback(), serverPort));
}

// Waits until the server has read `count` datagrams, then a little more so that its events are queued.
void waitForDatagrams(const rtype::server::AsioNetworkServer &server, std::uint64_t count)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (server.traffic().totals.packetsReceived < count && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

std::vector<std::uint8_t> drainInputs(rtype::server::AsioNetworkServer &server)
{
    std::vector<std::uint8_t> inputs;
    while (const auto event = server.poll()) {
        if (const auto *input = std::get_if<rtype::server::PlayerInput>(&*event))
            inputs.push_back(input->flags);
    }
    return inputs;
}

bool waitUntilClientCount(const rtype::server::AsioNetworkServer &server, std::size_t count)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (server.clientCount() != count && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    return server.clientCount() == count;
}

Udp::socket connectedClient(asio::io_context &io, std::uint16_t serverPort, std::uint32_t firstSequence)
{
    Udp::socket socket(io, Udp::endpoint(Udp::v4(), 0));
    socket.non_blocking(true);
    sendFrom(socket, serverPort, firstSequence, rtype::protocol::Connect{});
    Udp::endpoint sender;
    const auto accepted = receivePacket(socket, sender);
    EXPECT_TRUE(accepted && std::holds_alternative<rtype::protocol::ConnectAccepted>(accepted->message));
    return socket;
}

} // namespace

TEST(NetworkIntegration, NumbersThePacketsOfEachClientSeparately)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t serverPort = findAvailablePort();
    server.start(serverPort);

    asio::io_context io;
    Udp::socket first = connectedClient(io, serverPort, 1);
    Udp::socket second = connectedClient(io, serverPort, 1);
    for (std::uint32_t tick = 1; tick <= 3; ++tick) {
        server.broadcast(rtype::protocol::Snapshot{tick, rtype::protocol::GameState::Playing, 0, {}});
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    for (Udp::socket *socket : {&first, &second}) {
        Udp::endpoint sender;
        for (std::uint32_t expected = 1; expected <= 3; ++expected) {
            const auto packet = receivePacket(*socket, sender);
            ASSERT_TRUE(packet);
            // The ConnectAccepted was number 0 for this client, whatever the other clients received.
            EXPECT_EQ(packet->sequence, expected);
        }
    }
}

TEST(NetworkIntegration, IgnoresDuplicatedStaleAndTooOldInputs)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t serverPort = findAvailablePort();
    server.start(serverPort);

    asio::io_context io;
    Udp::socket client = connectedClient(io, serverPort, 1);
    using rtype::protocol::Input;
    namespace Flag = rtype::protocol::InputFlag;
    sendFrom(client, serverPort, 10, Input{Flag::Up});
    sendFrom(client, serverPort, 9, Input{Flag::Left});    // arrives after number 10: obsolete
    sendFrom(client, serverPort, 10, Input{Flag::Up});     // a copy of the first
    sendFrom(client, serverPort, 11, Input{Flag::Right});
    sendFrom(client, serverPort, 60, Input{Flag::Down});
    sendFrom(client, serverPort, 20, Input{Flag::Left});   // 40 numbers behind the highest: too old
    waitForDatagrams(server, 7);

    EXPECT_EQ(drainInputs(server), (std::vector<std::uint8_t>{Flag::Up, Flag::Right, Flag::Down}));
    EXPECT_EQ(server.traffic().totals.duplicatesDropped, 1u);
    EXPECT_EQ(server.traffic().totals.outdatedDropped, 2u);
}

TEST(NetworkIntegration, StillAnswersAPingThatArrivesOutOfOrder)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t serverPort = findAvailablePort();
    server.start(serverPort);

    asio::io_context io;
    Udp::socket client = connectedClient(io, serverPort, 1);
    sendFrom(client, serverPort, 10, rtype::protocol::Input{0});
    sendFrom(client, serverPort, 8, rtype::protocol::Ping{77}); // late, but nothing newer replaces a Ping

    Udp::endpoint sender;
    const auto pong = receivePacket(client, sender);
    ASSERT_TRUE(pong);
    const auto *answer = std::get_if<rtype::protocol::Pong>(&pong->message);
    ASSERT_NE(answer, nullptr);
    EXPECT_EQ(answer->timestampMs, 77u);
}

TEST(NetworkIntegration, AClientThatConnectsAgainStartsANewSequence)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t serverPort = findAvailablePort();
    server.start(serverPort);

    asio::io_context io;
    Udp::socket client = connectedClient(io, serverPort, 100);
    sendFrom(client, serverPort, 105, rtype::protocol::Input{rtype::protocol::InputFlag::Up});

    // The program restarts on the same address and numbers its packets from 1 again.
    sendFrom(client, serverPort, 1, rtype::protocol::Connect{});
    Udp::endpoint sender;
    ASSERT_TRUE(receivePacket(client, sender));
    sendFrom(client, serverPort, 2, rtype::protocol::Input{rtype::protocol::InputFlag::Down});
    waitForDatagrams(server, 4);

    EXPECT_EQ(drainInputs(server),
              (std::vector<std::uint8_t>{rtype::protocol::InputFlag::Up, rtype::protocol::InputFlag::Down}));
}

namespace {

// Reads datagrams until `limit` has passed, or `count` of them were collected.
std::vector<rtype::protocol::Packet> collectPackets(Udp::socket &socket, std::chrono::milliseconds limit,
                                                    std::size_t count = 100)
{
    std::vector<rtype::protocol::Packet> packets;
    std::array<std::uint8_t, rtype::protocol::MaxPacketSize> buffer{};
    const auto deadline = std::chrono::steady_clock::now() + limit;
    Udp::endpoint sender;
    while (std::chrono::steady_clock::now() < deadline && packets.size() < count) {
        asio::error_code error;
        const std::size_t size = socket.receive_from(asio::buffer(buffer), sender, 0, error);
        if (error) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        if (const auto packet = rtype::protocol::decode(std::span(buffer).first(size)))
            packets.push_back(*packet);
    }
    return packets;
}

void sendAck(Udp::socket &socket, std::uint16_t serverPort, std::uint32_t sequence, std::uint32_t reliableId)
{
    sendFrom(socket, serverPort, sequence, rtype::protocol::Ack{reliableId});
}

} // namespace

TEST(NetworkIntegration, RepeatsAReliableMessageUntilItIsAcknowledged)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t serverPort = findAvailablePort();
    server.start(serverPort);

    asio::io_context io;
    Udp::socket client = connectedClient(io, serverPort, 1);
    server.broadcastReliable(rtype::protocol::EntityDestroyed{42, rtype::protocol::DestroyReason::Killed});

    // The client stays silent: the same message comes again, in a new packet.
    const auto copies = collectPackets(client, std::chrono::milliseconds(700));
    ASSERT_GE(copies.size(), 2u);
    for (const auto &copy : copies) {
        ASSERT_TRUE(copy.reliableId);
        EXPECT_EQ(*copy.reliableId, *copies[0].reliableId);
        const auto *destroyed = std::get_if<rtype::protocol::EntityDestroyed>(&copy.message);
        ASSERT_NE(destroyed, nullptr);
        EXPECT_EQ(destroyed->netId, 42u);
    }
    EXPECT_NE(copies[1].sequence, copies[0].sequence) << "each copy is a packet of its own";
    EXPECT_GE(server.traffic().totals.retransmissions, 1u);

    // Once acknowledged, it is not sent again.
    sendAck(client, serverPort, 2, *copies[0].reliableId);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    collectPackets(client, std::chrono::milliseconds(20)); // copies already on their way
    EXPECT_TRUE(collectPackets(client, std::chrono::milliseconds(700)).empty());
}

TEST(NetworkIntegration, AcknowledgesAReliableDisconnectEvenWhenItIsRepeated)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t serverPort = findAvailablePort();
    server.start(serverPort);

    asio::io_context io;
    Udp::socket client = connectedClient(io, serverPort, 1);
    ASSERT_TRUE(waitUntilClientCount(server, 1));

    const rtype::protocol::Packet disconnect{2, rtype::protocol::Disconnect{}, 0u};
    client.send_to(asio::buffer(rtype::protocol::encode(disconnect)),
                   Udp::endpoint(asio::ip::address_v4::loopback(), serverPort));
    auto answers = collectPackets(client, std::chrono::milliseconds(1000), 1);
    ASSERT_EQ(answers.size(), 1u);
    const auto *ack = std::get_if<rtype::protocol::Ack>(&answers[0].message);
    ASSERT_NE(ack, nullptr);
    EXPECT_EQ(ack->reliableId, 0u);
    EXPECT_TRUE(waitUntilClientCount(server, 0));

    // The Ack was lost: the client repeats its Disconnect. The server forgot it, but must still answer.
    client.send_to(asio::buffer(rtype::protocol::encode({3, rtype::protocol::Disconnect{}, 0u})),
                   Udp::endpoint(asio::ip::address_v4::loopback(), serverPort));
    answers = collectPackets(client, std::chrono::milliseconds(1000), 1);
    ASSERT_EQ(answers.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<rtype::protocol::Ack>(answers[0].message));
}

TEST(NetworkIntegration, TellsTheOtherClientsReliablyThatAPlayerLeft)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t serverPort = findAvailablePort();
    server.start(serverPort);

    asio::io_context io;
    Udp::socket leaving = connectedClient(io, serverPort, 1);
    Udp::socket staying = connectedClient(io, serverPort, 1);
    sendFrom(leaving, serverPort, 2, rtype::protocol::Disconnect{});

    const auto notices = collectPackets(staying, std::chrono::milliseconds(700));
    ASSERT_GE(notices.size(), 2u) << "PlayerLeft must be repeated until acknowledged";
    for (const auto &notice : notices) {
        ASSERT_TRUE(notice.reliableId);
        const auto *left = std::get_if<rtype::protocol::PlayerLeft>(&notice.message);
        ASSERT_NE(left, nullptr);
        EXPECT_EQ(left->playerId, 0);
    }
    sendAck(staying, serverPort, 2, *notices[0].reliableId);
    collectPackets(staying, std::chrono::milliseconds(100));
    EXPECT_TRUE(collectPackets(staying, std::chrono::milliseconds(700)).empty());
}

TEST(NetworkIntegration, TheGameSeesAReliableInputOnlyOnce)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t serverPort = findAvailablePort();
    server.start(serverPort);

    asio::io_context io;
    Udp::socket client = connectedClient(io, serverPort, 1);
    const auto send = [&](std::uint32_t sequence) {
        client.send_to(asio::buffer(rtype::protocol::encode(
                           {sequence, rtype::protocol::Input{rtype::protocol::InputFlag::Up}, 7u})),
                       Udp::endpoint(asio::ip::address_v4::loopback(), serverPort));
    };
    send(2);
    send(3); // the same reliable message again, as the server's Ack never came
    waitForDatagrams(server, 3);

    EXPECT_EQ(drainInputs(server), (std::vector<std::uint8_t>{rtype::protocol::InputFlag::Up}));
    const auto acks = collectPackets(client, std::chrono::milliseconds(200));
    int ackCount = 0;
    for (const auto &packet : acks)
        ackCount += std::holds_alternative<rtype::protocol::Ack>(packet.message);
    EXPECT_EQ(ackCount, 2) << "every copy is acknowledged, in case the previous Ack was lost";
}
