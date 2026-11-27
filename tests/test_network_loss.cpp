#include "common/Protocol.hpp"
#include "net/AsioNetworkClient.hpp"
#include "net/AsioNetworkServer.hpp"

#include <asio.hpp>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <set>
#include <thread>

namespace {

using namespace std::chrono_literals;
using rtype::client::AsioNetworkClient;
using Udp = asio::ip::udp;

std::uint16_t findAvailablePort()
{
    asio::io_context io;
    Udp::socket probe(io, Udp::endpoint(Udp::v4(), 0));
    const auto port = probe.local_endpoint().port();
    probe.close();
    return port;
}

bool waitUntil(const std::function<bool()> &condition, std::chrono::milliseconds limit)
{
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (std::chrono::steady_clock::now() < deadline) {
        if (condition())
            return true;
        std::this_thread::sleep_for(5ms);
    }
    return condition();
}

// A UDP relay between one client and the server that loses a share of the datagrams, in both directions: the
// in-process equivalent of scripts/netsim.py --loss. The client connects to port(); the server sees the relay.
class LossyRelay {
public:
    LossyRelay(std::uint16_t serverPort, double lossPercent, unsigned seed)
        : _front(_io, Udp::endpoint(Udp::v4(), 0)), _back(_io, Udp::endpoint(Udp::v4(), 0)),
          _server(asio::ip::address_v4::loopback(), serverPort), _random(seed), _loss(lossPercent / 100.0)
    {
        receiveFromClient();
        receiveFromServer();
        _thread = std::thread([this] { _io.run(); });
    }

    ~LossyRelay()
    {
        _io.stop();
        _thread.join();
    }

    std::uint16_t port() const { return _front.local_endpoint().port(); }
    std::size_t dropped() const { return _dropped; }
    std::size_t forwarded() const { return _forwarded; }

private:
    bool lost() { return std::uniform_real_distribution<double>(0.0, 1.0)(_random) < _loss; }

    void receiveFromClient()
    {
        _front.async_receive_from(asio::buffer(_clientBuffer), _client,
                                  [this](const asio::error_code &error, std::size_t size) {
                                      if (!error) {
                                          _clientKnown = true;
                                          if (lost()) {
                                              ++_dropped;
                                          } else {
                                              ++_forwarded;
                                              _back.send_to(asio::buffer(_clientBuffer.data(), size), _server);
                                          }
                                      }
                                      receiveFromClient();
                                  });
    }

    void receiveFromServer()
    {
        _back.async_receive_from(asio::buffer(_serverBuffer), _sender,
                                 [this](const asio::error_code &error, std::size_t size) {
                                     if (!error && _clientKnown) {
                                         if (lost()) {
                                             ++_dropped;
                                         } else {
                                             ++_forwarded;
                                             _front.send_to(asio::buffer(_serverBuffer.data(), size), _client);
                                         }
                                     }
                                     receiveFromServer();
                                 });
    }

    asio::io_context _io;
    Udp::socket _front;
    Udp::socket _back;
    Udp::endpoint _server;
    Udp::endpoint _client;
    Udp::endpoint _sender;
    std::array<std::uint8_t, rtype::protocol::MaxPacketSize> _clientBuffer{};
    std::array<std::uint8_t, rtype::protocol::MaxPacketSize> _serverBuffer{};
    std::mt19937 _random;
    double _loss;
    bool _clientKnown = false;
    std::size_t _dropped = 0;
    std::size_t _forwarded = 0;
    std::thread _thread;
};

// Patient settings for a link that loses packets: many Connect attempts, a long silence tolerance.
AsioNetworkClient::Options lossyLinkOptions()
{
    AsioNetworkClient::Options options;
    options.connectRetryInterval = 100ms;
    options.maxConnectAttempts = 40;
    options.serverTimeout = 8000ms;
    options.pingInterval = 200ms;
    return options;
}

} // namespace

TEST(NetworkLoss, EveryReliableMessageArrivesExactlyOnceThroughAThirtyPercentLossyLink)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t serverPort = findAvailablePort();
    server.start(serverPort);
    LossyRelay relay(serverPort, 30.0, 12345);

    AsioNetworkClient client;
    client.setOptions(lossyLinkOptions());
    client.connect("127.0.0.1", relay.port());
    ASSERT_TRUE(waitUntil([&] { return client.connected(); }, 8000ms)) << "Connect is repeated until it gets through";

    constexpr std::uint32_t MessageCount = 60;
    for (std::uint32_t netId = 1; netId <= MessageCount; ++netId)
        server.broadcastReliable(rtype::protocol::EntityDestroyed{netId, rtype::protocol::DestroyReason::Killed});

    std::multiset<std::uint32_t> delivered;
    const auto collect = [&] {
        while (const auto message = client.poll()) {
            if (const auto *destroyed = std::get_if<rtype::protocol::EntityDestroyed>(&*message))
                delivered.insert(destroyed->netId);
        }
        return delivered.size() >= MessageCount;
    };
    EXPECT_TRUE(waitUntil(collect, 15000ms)) << "only " << delivered.size() << " of " << MessageCount << " arrived";

    // Let the copies still in flight arrive: none of them may be handed to the game a second time.
    std::this_thread::sleep_for(600ms);
    collect();
    EXPECT_EQ(delivered.size(), MessageCount);
    EXPECT_EQ(std::set<std::uint32_t>(delivered.begin(), delivered.end()).size(), MessageCount);

    EXPECT_GT(relay.dropped(), 10u) << "the link really lost packets";
    EXPECT_GT(server.traffic().totals.retransmissions, 0u) << "so the server had to repeat some messages";
    EXPECT_GT(client.stats().traffic.totals.duplicatesDropped, 0u) << "and the client ignored the copies";
}

TEST(NetworkLoss, TheMeasuredLossFollowsTheLossOfTheLink)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t serverPort = findAvailablePort();
    server.start(serverPort);
    LossyRelay relay(serverPort, 30.0, 777);

    AsioNetworkClient client;
    client.setOptions(lossyLinkOptions());
    client.connect("127.0.0.1", relay.port());
    ASSERT_TRUE(waitUntil([&] { return client.connected(); }, 8000ms));

    // 400 snapshots, each a packet of its own numbered by the server.
    for (std::uint32_t tick = 2; tick <= 800; tick += 2) {
        server.broadcast(rtype::protocol::Snapshot{tick, rtype::protocol::GameState::Playing, 0, {}, {}});
        std::this_thread::sleep_for(1ms);
    }
    std::this_thread::sleep_for(500ms); // one more round of statistics

    // 30 % of the packets are lost, so the estimate should be near 30 %; a few hundred packets leave a margin.
    const double measured = client.stats().lossPercent;
    EXPECT_GT(measured, 18.0);
    EXPECT_LT(measured, 42.0);
}

TEST(NetworkLoss, ADisconnectEndsTheSessionEvenWhenPacketsAreLost)
{
    rtype::server::AsioNetworkServer server;
    const std::uint16_t serverPort = findAvailablePort();
    server.start(serverPort);
    LossyRelay relay(serverPort, 30.0, 4242);

    AsioNetworkClient client;
    auto options = lossyLinkOptions();
    options.closeTimeout = 3000ms; // long enough for a few repeats
    client.setOptions(options);
    client.connect("127.0.0.1", relay.port());
    ASSERT_TRUE(waitUntil([&] { return client.connected(); }, 8000ms));
    ASSERT_TRUE(waitUntil([&] { return server.clientCount() == 1; }, 2000ms));

    client.disconnect();
    EXPECT_TRUE(waitUntil([&] { return server.clientCount() == 0; }, 2000ms))
        << "the Disconnect was repeated until it got through, long before the server's own timeout";
}
