#include "ServerLoop.hpp"

#include "engine/core/FixedTimestep.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <thread>
#include <type_traits>
#include <variant>

namespace rtype::server {

namespace {

void handleNetworkEvents(Game &game, INetworkServer &network)
{
    while (auto event = network.poll()) {
        std::visit(
            [&game](const auto &networkEvent) {
                using Event = std::decay_t<decltype(networkEvent)>;
                if constexpr (std::is_same_v<Event, PlayerJoined>)
                    game.onPlayerJoined(networkEvent.id);
                else if constexpr (std::is_same_v<Event, PlayerDisconnected>)
                    game.onPlayerLeft(networkEvent.id);
                else if constexpr (std::is_same_v<Event, PlayerInput>)
                    game.onInput(networkEvent.id, networkEvent.flags);
            },
            *event);
    }
}

void step(Game &game, INetworkServer &network, const ServerSettings &settings, float seconds)
{
    handleNetworkEvents(game, network);
    game.update(seconds);
    if (game.tick() % static_cast<std::uint32_t>(settings.snapshotEvery) == 0)
        network.broadcast(game.snapshot());
    for (const auto &message : game.takeEvents())
        network.broadcastReliable(message);
}

} // namespace

void configureGame(Game &game, const ServerSettings &settings)
{
    game.setGameMode(settings.mode);
    game.setDifficulty(settings.difficulty);
    if (!settings.level.empty())
        game.loadLevel(settings.level);
    game.checkLevels();
    game.startAtLevel(settings.startLevel);
}

void runServer(Game &game, INetworkServer &network, const ServerSettings &settings, const std::atomic<bool> &running,
               const std::function<void()> &everySecond)
{
    engine::core::FixedTimestep clock(TickRate);
    auto nextSecond = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (running) {
        for (int ticks = clock.advance(); ticks > 0; --ticks) {
            try {
                step(game, network, settings, clock.step());
            } catch (const std::exception &error) {
                std::cerr << "r-type_server: tick " << game.tick() << " skipped: " << error.what() << '\n';
            }
        }
        if (everySecond && std::chrono::steady_clock::now() >= nextSecond) {
            everySecond();
            nextSecond += std::chrono::seconds(1);
        }
        std::this_thread::sleep_for(clock.untilNextTick());
    }
}

} // namespace rtype::server
