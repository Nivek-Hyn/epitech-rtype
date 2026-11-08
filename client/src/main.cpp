#include "Game.hpp"
#include "ResourceManager.hpp"
#include "common/Constants.hpp"
#include "common/Protocol.hpp"
#include "engine/core/FixedTimestep.hpp"
#include "game/Session.hpp"
#include "net/AsioNetworkClient.hpp"
#include "net/AsioNetworkServer.hpp"
#include "ui/MainMenu.hpp"
#include "ui/Starfield.hpp"

#include <SFML/Graphics.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace rtype::client;

struct Arguments {
    std::string host = "127.0.0.1";
    std::uint16_t port = rtype::DefaultPort;
    bool directConnect = false; // a host was given: skip the menu
};

// One line of network statistics: ping, bandwidth, loss, dropped and resent packets.
std::string statsLine(const NetworkStats &stats)
{
    char rtt[24] = "-- ms";
    if (stats.rttMs)
        std::snprintf(rtt, sizeof rtt, "%.0f ms", *stats.rttMs);
    char line[200];
    const auto dropped = stats.traffic.totals.duplicatesDropped + stats.traffic.totals.outdatedDropped;
    std::snprintf(line, sizeof line,
                  "ping %s | down %.1f KB/s | up %.1f KB/s | loss %.1f%% | dropped %llu | resent %llu  [F3]", rtt,
                  stats.traffic.rates.receivedBytesPerSecond / 1024.0, stats.traffic.rates.sentBytesPerSecond / 1024.0,
                  stats.lossPercent, static_cast<unsigned long long>(dropped),
                  static_cast<unsigned long long>(stats.traffic.totals.retransmissions));
    return line;
}

std::optional<Arguments> parseArguments(int argc, char **argv)
{
    Arguments arguments;
    try {
        if (argc > 1) {
            arguments.host = argv[1];
            arguments.directConnect = true;
        }
        if (argc > 2)
            arguments.port = static_cast<std::uint16_t>(std::stoul(argv[2]));
    } catch (const std::exception &) {
        return std::nullopt;
    }
    return arguments;
}

// Runs an embedded local server in a background thread for Solo and Local Host.
class LocalServer {
public:
    ~LocalServer() { stop(); }

    void start(std::uint16_t port, const std::string &mode = "campaign", const std::string &difficulty = "normal",
               const std::string &level = "")
    {
        stop();
        _running = true;
        _thread = std::thread([this, port, mode, difficulty, level]() { run(port, mode, difficulty, level); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void stop()
    {
        _running = false;
        if (_thread.joinable())
            _thread.join();
    }

private:
    void run(std::uint16_t port, const std::string &mode, const std::string &difficulty, const std::string &level)
    {
        try {
            rtype::server::Game game;
            game.setGameMode(mode == "infinite" ? rtype::server::Game::GameMode::Infinite
                                                : rtype::server::Game::GameMode::Campaign);
            if (difficulty == "easy")
                game.setDifficulty(rtype::server::Game::Difficulty::Easy);
            else if (difficulty == "hard")
                game.setDifficulty(rtype::server::Game::Difficulty::Hard);
            else
                game.setDifficulty(rtype::server::Game::Difficulty::Normal);

            if (!level.empty()) {
                try {
                    game.loadLevel(level);
                } catch (...) {
                }
            }

            rtype::server::AsioNetworkServer network;
            network.start(port);

            engine::core::FixedTimestep clock(rtype::TickRate);
            while (_running) {
                for (int i = clock.advance(); i > 0; --i) {
                    while (auto event = network.poll()) {
                        std::visit(
                            [&game](const auto &e) {
                                using Event = std::decay_t<decltype(e)>;
                                if constexpr (std::is_same_v<Event, rtype::server::PlayerJoined>)
                                    game.onPlayerJoined(e.id);
                                else if constexpr (std::is_same_v<Event, rtype::server::PlayerDisconnected>)
                                    game.onPlayerLeft(e.id);
                                else if constexpr (std::is_same_v<Event, rtype::server::PlayerInput>)
                                    game.onInput(e.id, e.flags);
                            },
                            *event);
                    }
                    game.update(clock.step());
                    if (game.tick() % rtype::SnapshotEveryTicks == 0)
                        network.broadcast(game.snapshot());
                    for (const auto &message : game.takeEvents())
                        network.broadcastReliable(message);
                }
                std::this_thread::sleep_for(clock.untilNextTick());
            }
            network.stop();
        } catch (const std::exception &e) {
            std::cerr << "[LocalServer] Stopped: " << e.what() << '\n';
        }
    }

    std::atomic<bool> _running{false};
    std::thread _thread;
};

// The client: the menu, then a game session, back to the menu with Escape.
class App {
public:
    App(ResourceManager &resources, bool hasAssets)
        : _window(sf::VideoMode({1280u, 720u}), "R-Type"), _resources(resources), _hasAssets(hasAssets),
          _menu(resources.getFont("default"), &resources), _session(resources)
    {
        _window.setFramerateLimit(60);
        const auto &config = _menu.getConfig();
        _lastSfxVolume = config.sfxVolume;
        _resources.setSfxVolume(config.sfxVolume);
        _resources.setMusicVolume(config.musicVolume);
        _resources.playMusic("assets/audio/music.wav", config.musicVolume, true);
    }

    void connect(const std::string &host, std::uint16_t port)
    {
        _network.connect(host, port);
        _session.reset();
        _playing = true;
    }

    void run()
    {
        sf::Clock clock;
        while (_window.isOpen()) {
            const float dt = clock.restart().asSeconds();
            _time += dt;
            applyVolumes();
            handleEvents();
            if (_playing)
                updateGame(dt);
            else
                updateMenu(dt);
            _resources.updateAudio();
            draw(dt);
        }
        if (_network.connected())
            _network.disconnect();
        _server.stop();
        _menu.getConfig().saveToFile("config.json");
    }

private:
    // Volumes follow the settings; a short sound lets the player hear the new effects volume.
    void applyVolumes()
    {
        const auto &config = _menu.getConfig();
        _resources.setSfxVolume(config.sfxVolume);
        _resources.setMusicVolume(config.musicVolume);
        if (std::abs(config.sfxVolume - _lastSfxVolume) > 1.f) {
            _resources.playSound("shoot", 80.f);
            _lastSfxVolume = config.sfxVolume;
        }
    }

    void handleEvents()
    {
        while (const std::optional<sf::Event> event = _window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                _window.close();
            if (!_playing) {
                _menu.handleEvent(*event, _window);
            } else if (const auto *key = event->getIf<sf::Event::KeyPressed>()) {
                const auto &config = _menu.getConfig();
                if (key->code == sf::Keyboard::Key::F1)
                    _menu.setAccessibility(!config.autoFire, config.highContrast);
                else if (key->code == sf::Keyboard::Key::F2)
                    _menu.setAccessibility(config.autoFire, !config.highContrast);
                else if (key->code == sf::Keyboard::Key::F3)
                    _showStats = !_showStats;
                else if (key->code == sf::Keyboard::Key::Escape)
                    leaveToMenu();
                else if (_session.isWaiting() &&
                         (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space))
                    _session.setReady(true);
            } else if (_session.isWaiting() && event->is<sf::Event::MouseButtonPressed>()) {
                _session.setReady(true);
            }
        }
    }

    void updateMenu(float dt)
    {
        _menu.update(dt);
        const auto action = _menu.getAndResetAction();
        if (action == ui::MenuAction::Exit) {
            _window.close();
        } else if (action == ui::MenuAction::Play) {
            try {
                _server.start(rtype::DefaultPort, "campaign", "normal", "assets/levels/level1.json");
                connect("127.0.0.1", rtype::DefaultPort);
            } catch (const std::exception &e) {
                _menu.setStatusMessage("Failed to start local game: " + std::string(e.what()), sf::Color::Red);
            }
        } else if (action == ui::MenuAction::Host) {
            const auto &config = _menu.getConfig();
            try {
                const std::uint16_t port = static_cast<std::uint16_t>(
                    config.hostPort.empty() ? rtype::DefaultPort : std::stoul(config.hostPort));
                const std::string mode = config.gameMode == 0 ? "campaign" : "infinite";
                const std::string diff =
                    config.difficulty == 0 ? "easy" : (config.difficulty == 1 ? "normal" : "hard");
                const std::string lvl =
                    mode == "campaign" ? "assets/levels/level" + std::to_string(config.mission + 1) + ".json" : "";
                _server.start(port, mode, diff, lvl);
                connect("127.0.0.1", port);
            } catch (const std::exception &e) {
                _menu.setStatusMessage("Failed to host server: " + std::string(e.what()), sf::Color::Red);
            }
        } else if (action == ui::MenuAction::Connect) {
            const auto &config = _menu.getConfig();
            try {
                connect(config.joinIp, static_cast<std::uint16_t>(std::stoul(config.joinPort)));
            } catch (const std::exception &e) {
                _menu.setStatusMessage("Connection failed: " + std::string(e.what()), sf::Color::Red);
            }
        }
    }

    void updateGame(float dt)
    {
        _starfield.update(dt);
        _session.update(dt, _network.connected());
        _session.setHighContrast(_menu.getConfig().highContrast);
        _session.sendInput(_network, _menu.getConfig(), dt, _window.hasFocus());
        _session.receive(_network);
        checkConnection();
    }

    // A refused, unreachable or lost server sends the player back to the menu, which explains why.
    void checkConnection()
    {
        switch (_network.state()) {
        case ConnectionState::Rejected:
            leaveToMenu("Connection rejected: " + _session.rejectionReason() + ".");
            break;
        case ConnectionState::TimedOut:
            leaveToMenu("Could not reach the server. Check the address and that the server is running.");
            break;
        case ConnectionState::Lost:
            leaveToMenu("Connection to the server lost.");
            break;
        default:
            break;
        }
    }

    void leaveToMenu(const std::string &message = "")
    {
        _network.disconnect();
        _server.stop();
        _playing = false;
        _session.reset();
        if (!message.empty())
            _menu.setStatusMessage(message, sf::Color::Red);
    }

    // Bottom left, away from the HUD.
    void drawStats()
    {
        sf::Text text(_resources.getFont("default"), statsLine(_network.stats()), 22);
        text.setFillColor(sf::Color::White);
        text.setOutlineColor(sf::Color::Black);
        text.setOutlineThickness(2.f);
        text.setPosition({20.f, rtype::WorldHeight - 45.f});
        _window.draw(text);
    }

    // Everything is drawn in world coordinates (1920x1080); the view scales them to the window.
    void draw(float dt)
    {
        _window.clear(sf::Color(10, 10, 25));
        if (_playing) {
            _window.setView(_session.view());
            if (!_menu.getConfig().highContrast) // plain dark background in high contrast
                _starfield.draw(_window);
            _session.draw(_window, _time, dt, _hasAssets);
            if (_showStats)
                drawStats();
        } else {
            _window.setView(sf::View(sf::FloatRect({0.f, 0.f}, {rtype::WorldWidth, rtype::WorldHeight})));
            _menu.draw(_window);
        }
        _window.display();
    }

    sf::RenderWindow _window;
    ResourceManager &_resources;
    bool _hasAssets;
    ui::Starfield _starfield;
    ui::MainMenu _menu;
    AsioNetworkClient _network;
    Session _session;
    LocalServer _server;
    bool _playing = false;
    bool _showStats = false; // network statistics, toggled with F3
    float _time = 0.f;
    float _lastSfxVolume = 0.f;
};

} // namespace

int main(int argc, char **argv)
{
    const std::optional<Arguments> arguments = parseArguments(argc, argv);
    if (!arguments) {
        std::cerr << "Usage: " << argv[0] << " [host] [port]\n";
        return 84;
    }

    ResourceManager resources;
    const bool hasAssets = resources.loadDefaults();
    if (!hasAssets)
        std::cerr << "Warning: Could not load some assets. Make sure to run from repository root.\n";
    if (!resources.hasFont("default")) {
        std::cerr << "Error: Default font could not be loaded.\n";
        return 84;
    }

    App app(resources, hasAssets);
    if (arguments->directConnect) {
        try {
            app.connect(arguments->host, arguments->port);
        } catch (const std::exception &e) {
            std::cerr << "Direct connect failed: " << e.what() << '\n';
        }
    }
    app.run();
    return EXIT_SUCCESS;
}
