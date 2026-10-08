#include "Game.hpp"

#include "common/Constants.hpp"
#include "engine/core/FixedTimestep.hpp"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

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
    // TODO(network): create the Asio implementation of INetworkServer and start(port).
    std::cout << "r-type_server: listening on UDP port " << port << " (network not implemented yet)\n";

    engine::core::FixedTimestep clock(rtype::TickRate);
    while (g_running) {
        for (int i = clock.advance(); i > 0; --i) {
            // TODO(network): while (auto event = network.poll()) -> game.onPlayerJoined / onPlayerLeft / onInput
            game.update(clock.step());
            // TODO(network): network.broadcast(game.snapshot()) and every game.takeEvents()
        }
        std::this_thread::sleep_for(clock.untilNextTick());
    }

    std::cout << "r-type_server: stopped after " << game.tick() << " ticks\n";
    return EXIT_SUCCESS;
}
