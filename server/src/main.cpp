#include "Game.hpp"
#include "net/AsioNetworkServer.hpp"

#include "common/Constants.hpp"
#include "engine/core/FixedTimestep.hpp"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <thread>
#include <type_traits>
#include <variant>

namespace {

std::atomic<bool> g_running{true};

void onSignal(int)
{
    g_running = false;
}

} // namespace

int main(int argc, char **argv)
{
    std::uint16_t port = rtype::DefaultPort;
    if (argc > 1) {
        try {
            port = static_cast<std::uint16_t>(std::stoul(argv[1]));
        } catch (const std::exception &) {
            std::cerr << "Usage: " << argv[0] << " [port]\n";
            return 84;
        }
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    rtype::server::Game game;
    rtype::server::AsioNetworkServer network;
    try {
        network.start(port);
    } catch (const std::exception &error) {
        std::cerr << "Could not start UDP server: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "r-type_server: listening on UDP port " << port << '\n';

    engine::core::FixedTimestep clock(rtype::TickRate);
    while (g_running) {
        for (int i = clock.advance(); i > 0; --i) {
            while (auto event = network.poll()) {
                std::visit(
                    [&game](const auto &networkEvent) {
                        using Event = std::decay_t<decltype(networkEvent)>;
                        if constexpr (std::is_same_v<Event, rtype::server::PlayerJoined>)
                            game.onPlayerJoined(networkEvent.id);
                        else if constexpr (std::is_same_v<Event, rtype::server::PlayerDisconnected>)
                            game.onPlayerLeft(networkEvent.id);
                        else if constexpr (std::is_same_v<Event, rtype::server::PlayerInput>)
                            game.onInput(networkEvent.id, networkEvent.flags);
                    },
                    *event);
            }
            game.update(clock.step());
            network.broadcast(game.snapshot());
            for (const auto &message : game.takeEvents())
                network.broadcast(message);
        }
        std::this_thread::sleep_for(clock.untilNextTick());
    }

    network.stop();
    std::cout << "r-type_server: stopped after " << game.tick() << " ticks\n";
    return EXIT_SUCCESS;
}
