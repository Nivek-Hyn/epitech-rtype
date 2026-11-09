#include "Game.hpp"
#include "ServerLoop.hpp"
#include "net/AsioNetworkServer.hpp"

#include "common/Constants.hpp"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

std::atomic<bool> g_running{true};

void onSignal(int)
{
    g_running = false;
}

void printTraffic(const rtype::server::AsioNetworkServer &network)
{
    const auto stats = network.traffic();
    const std::size_t clients = network.clientCount();
    const double sentKb = stats.rates.sentBytesPerSecond / 1024.0;
    const double receivedKb = stats.rates.receivedBytesPerSecond / 1024.0;
    std::printf("[net] %zu client(s) | out %.1f KB/s (%.1f per client) | in %.1f KB/s | total out %.1f KB, in %.1f KB"
                " | dropped: %llu duplicate, %llu outdated, %llu over the rate limit | resent: %llu\n",
                clients, sentKb, clients > 0 ? sentKb / static_cast<double>(clients) : 0.0, receivedKb,
                static_cast<double>(stats.totals.bytesSent) / 1024.0,
                static_cast<double>(stats.totals.bytesReceived) / 1024.0,
                static_cast<unsigned long long>(stats.totals.duplicatesDropped),
                static_cast<unsigned long long>(stats.totals.outdatedDropped),
                static_cast<unsigned long long>(stats.totals.rateLimited),
                static_cast<unsigned long long>(stats.totals.retransmissions));
    std::fflush(stdout);
}

const char *const Usage = " [port] [level_path] [--mode campaign|infinite] [--difficulty easy|normal|hard]"
                          " [--level path] [--start-level index] [--stats] [--snapshot-every ticks]\n";

// A number in [minimum, maximum], or std::invalid_argument.
long parseNumber(const std::string &text, long minimum, long maximum, const std::string &what)
{
    std::size_t used = 0;
    long value = 0;
    try {
        value = std::stol(text, &used);
    } catch (const std::exception &) {
        used = 0;
    }
    if (used != text.size() || value < minimum || value > maximum)
        throw std::invalid_argument(what + " expects a number between " + std::to_string(minimum) + " and " +
                                    std::to_string(maximum));
    return value;
}

rtype::server::Game::GameMode parseMode(const std::string &text)
{
    if (text == "campaign")
        return rtype::server::Game::GameMode::Campaign;
    if (text == "infinite")
        return rtype::server::Game::GameMode::Infinite;
    throw std::invalid_argument("--mode expects campaign or infinite");
}

rtype::server::Game::Difficulty parseDifficulty(const std::string &text)
{
    if (text == "easy")
        return rtype::server::Game::Difficulty::Easy;
    if (text == "normal")
        return rtype::server::Game::Difficulty::Normal;
    if (text == "hard")
        return rtype::server::Game::Difficulty::Hard;
    throw std::invalid_argument("--difficulty expects easy, normal or hard");
}

struct Options {
    rtype::server::ServerSettings settings;
    bool showTraffic = false;
    bool help = false;
};

// Throws std::invalid_argument with the reason; an unknown argument is an error, not silently ignored.
Options parseArguments(int argc, char **argv)
{
    Options options;
    auto &settings = options.settings;
    const auto value = [&](int &i, const std::string &name) -> std::string {
        if (i + 1 >= argc)
            throw std::invalid_argument(name + " expects a value");
        return argv[++i];
    };
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--mode")
            settings.mode = parseMode(value(i, arg));
        else if (arg == "--difficulty")
            settings.difficulty = parseDifficulty(value(i, arg));
        else if (arg == "--level")
            settings.level = value(i, arg);
        else if (arg == "--start-level")
            settings.startLevel = static_cast<std::size_t>(parseNumber(value(i, arg), 0, 255, arg));
        else if (arg == "--snapshot-every")
            settings.snapshotEvery =
                static_cast<int>(parseNumber(value(i, arg), 1, static_cast<long>(rtype::TickRate), arg));
        else if (arg == "--stats")
            options.showTraffic = true;
        else if (arg == "-h" || arg == "--help")
            options.help = true;
        else if (i == 1 && arg.rfind("--", 0) != 0)
            settings.port = static_cast<std::uint16_t>(parseNumber(arg, 1, 65535, "the port"));
        else if (i == 2 && arg.rfind("--", 0) != 0 && settings.level.empty())
            settings.level = arg;
        else
            throw std::invalid_argument("unknown argument '" + arg + "'");
    }
    return options;
}

} // namespace

int main(int argc, char **argv)
{
    Options options;
    try {
        options = parseArguments(argc, argv);
    } catch (const std::exception &error) {
        std::cerr << "r-type_server: " << error.what() << "\nUsage: " << argv[0] << Usage;
        return 84;
    }
    if (options.help) {
        std::cout << "Usage: " << argv[0] << Usage;
        return 0;
    }
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    rtype::server::Game game;
    rtype::server::AsioNetworkServer network;
    try {
        rtype::server::configureGame(game, options.settings);
        network.start(options.settings.port);
    } catch (const std::exception &error) {
        std::cerr << "r-type_server: " << error.what() << '\n';
        return 84;
    }
    std::cout << "r-type_server: listening on UDP port " << options.settings.port << ", "
              << static_cast<int>(rtype::TickRate) / options.settings.snapshotEvery << " snapshots per second\n";

    int seconds = 0;
    rtype::server::runServer(game, network, options.settings, g_running, [&] {
        if (options.showTraffic && ++seconds % 2 == 0)
            printTraffic(network);
    });
    network.stop();
    std::cout << "r-type_server: stopped after " << game.tick() << " ticks\n";
    return EXIT_SUCCESS;
}
