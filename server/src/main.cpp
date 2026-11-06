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
    std::string modeStr = "campaign";
    std::string diffStr = "normal";
    std::string levelPath;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) {
            modeStr = argv[++i];
        } else if (arg == "--difficulty" && i + 1 < argc) {
            diffStr = argv[++i];
        } else if (arg == "--level" && i + 1 < argc) {
            levelPath = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            std::cout
                << "Usage: " << argv[0]
                << " [port] [level_path] [--mode campaign|infinite] [--difficulty easy|normal|hard] [--level path]\n";
            return 0;
        } else if (arg.rfind("--", 0) != 0 && i == 1) {
            try {
                port = static_cast<std::uint16_t>(std::stoul(arg));
            } catch (const std::exception &) {
                std::cerr << "Usage: " << argv[0]
                          << " [port] [level_path] [--mode campaign|infinite] [--difficulty easy|normal|hard] [--level "
                             "path]\n";
                return 84;
            }
        } else if (arg.rfind("--", 0) != 0 && i == 2 && levelPath.empty()) {
            levelPath = arg;
        }
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    rtype::server::Game game;
    if (modeStr == "infinite") {
        game.setGameMode(rtype::server::Game::GameMode::Infinite);
    } else {
        game.setGameMode(rtype::server::Game::GameMode::Campaign);
    }

    if (diffStr == "easy") {
        game.setDifficulty(rtype::server::Game::Difficulty::Easy);
    } else if (diffStr == "hard") {
        game.setDifficulty(rtype::server::Game::Difficulty::Hard);
    } else {
        game.setDifficulty(rtype::server::Game::Difficulty::Normal);
    }

    if (!levelPath.empty()) {
        try {
            game.loadLevel(levelPath);
            std::cout << "r-type_server: Loaded level: " << levelPath << '\n';
        } catch (const std::exception &e) {
            std::cerr << "Failed to load custom level: " << e.what() << '\n';
            return 84;
        }
    }
    rtype::server::AsioNetworkServer network;
    try {
        network.start(port);
    } catch (const std::exception &error) {
        std::cerr << "Could not start UDP server: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "r-type_server: listening on UDP port " << port
              << " [Mode: " << (game.gameMode() == rtype::server::Game::GameMode::Infinite ? "Infinite" : "Campaign")
              << ", Difficulty: " << diffStr << "]\n";

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
