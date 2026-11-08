#include "ResourceManager.hpp"
#include "common/Constants.hpp"
#include "common/Protocol.hpp"
#include "game/LocalServer.hpp"
#include "game/Session.hpp"
#include "net/AsioNetworkClient.hpp"
#include "ui/MainMenu.hpp"
#include "ui/Starfield.hpp"

#include <SFML/Graphics.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace rtype::client;
namespace server = rtype::server;

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

// The difficulty chosen in the host screen: 0 easy, 1 normal, 2 hard.
server::Game::Difficulty difficultyOf(int index)
{
    if (index == 0)
        return server::Game::Difficulty::Easy;
    return index == 2 ? server::Game::Difficulty::Hard : server::Game::Difficulty::Normal;
}

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

    // Starts the local server, then joins it. On failure, the menu shows why.
    void host(server::ServerSettings settings, const std::string &port = "")
    {
        try {
            if (!port.empty())
                settings.port = static_cast<std::uint16_t>(std::stoul(port));
            _server.start(settings);
            connect("127.0.0.1", settings.port);
        } catch (const std::exception &e) {
            _server.stop();
            _menu.setStatusMessage("Could not start the game: " + std::string(e.what()), sf::Color::Red);
        }
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
            host(server::ServerSettings{}); // the whole campaign, normal difficulty, default port
        } else if (action == ui::MenuAction::Host) {
            const auto &config = _menu.getConfig();
            server::ServerSettings settings;
            settings.mode = config.gameMode == 0 ? server::Game::GameMode::Campaign : server::Game::GameMode::Infinite;
            settings.difficulty = difficultyOf(config.difficulty);
            settings.startLevel = static_cast<std::size_t>(config.mission) * 3; // the first level of the world
            host(settings, config.hostPort);
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
