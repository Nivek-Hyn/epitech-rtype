#include "App.hpp"

#include "common/Constants.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <optional>

namespace rtype::client {

namespace {

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

// The difficulty chosen in a menu: 0 easy, 1 normal, 2 hard.
server::Game::Difficulty difficultyOf(int index)
{
    if (index == 0)
        return server::Game::Difficulty::Easy;
    return index == 2 ? server::Game::Difficulty::Hard : server::Game::Difficulty::Normal;
}

} // namespace

App::App(ResourceManager &resources, bool hasAssets)
    : _window(sf::VideoMode({1280u, 720u}), "R-Type"), _resources(resources), _hasAssets(hasAssets),
      _menu(resources.getFont("default"), &resources), _campaign(resources.getFont("default")),
      _editor(resources, resources.getFont("default")), _session(resources)
{
    _window.setFramerateLimit(60);
    const auto &config = _menu.getConfig();
    _lastSfxVolume = config.sfxVolume;
    _resources.setSfxVolume(config.sfxVolume);
    _resources.setMusicVolume(config.musicVolume);
    _resources.playMusic("assets/audio/music.wav", config.musicVolume, true);
}

void App::connect(const std::string &host, std::uint16_t port)
{
    _network.connect(host, port);
    _session.reset();
    _screen = Screen::Playing;
}

// Starts the local server, then joins it. On failure, the menu shows why.
void App::host(server::ServerSettings settings, const std::string &port)
{
    try {
        if (!port.empty())
            settings.port = static_cast<std::uint16_t>(std::stoul(port));
        _server.start(settings);
        connect("127.0.0.1", settings.port);
        if (settings.noLobby)
            _session.setReady(true);
    } catch (const std::exception &e) {
        _server.stop();
        _screen = Screen::Menu;
        _menu.setStatusMessage("Could not start the game: " + std::string(e.what()), sf::Color::Red);
    }
}

void App::run()
{
    sf::Clock clock;
    while (_window.isOpen()) {
        const float dt = clock.restart().asSeconds();
        _time += dt;
        float bgSpeedFactor = 1.0f;
        if (_screen == Screen::Playing) {
            const int diff = _menu.getConfig().difficulty;
            bgSpeedFactor = diff == 0 ? 0.8f : (diff == 2 ? 1.45f : 1.0f);
        }
        _background.update(dt * bgSpeedFactor);
        applyVolumes();
        handleEvents();
        if (_screen == Screen::Playing)
            updateGame(dt);
        else if (_screen == Screen::Campaign)
            updateCampaign(dt);
        else if (_screen == Screen::Editor)
            updateEditor(dt);
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

// Volumes follow the settings; a short sound lets the player hear the new effects volume.
void App::applyVolumes()
{
    const auto &config = _menu.getConfig();
    _resources.setSfxVolume(config.sfxVolume);
    _resources.setMusicVolume(config.musicVolume);
    if (std::abs(config.sfxVolume - _lastSfxVolume) > 1.f) {
        _resources.playSound("shoot", 80.f);
        _lastSfxVolume = config.sfxVolume;
    }
}

void App::handleEvents()
{
    while (const std::optional<sf::Event> event = _window.pollEvent()) {
        if (event->is<sf::Event::Closed>())
            _window.close();
        else if (_screen == Screen::Menu)
            _menu.handleEvent(*event, _window);
        else if (_screen == Screen::Campaign)
            _campaign.handleEvent(*event, _window);
        else if (_screen == Screen::Editor)
            _editor.handleEvent(*event, _window);
        else if (const auto *key = event->getIf<sf::Event::KeyPressed>())
            handleGameKey(key->code);
        else if (_session.isWaiting() && event->is<sf::Event::MouseButtonPressed>())
            _session.setReady(true);
    }
}

void App::handleGameKey(sf::Keyboard::Key key)
{
    const auto &config = _menu.getConfig();
    if (key == sf::Keyboard::Key::F1)
        _menu.setAccessibility(!config.autoFire, config.highContrast);
    else if (key == sf::Keyboard::Key::F2)
        _menu.setAccessibility(config.autoFire, !config.highContrast);
    else if (key == sf::Keyboard::Key::F3)
        _showStats = !_showStats;
    else if (key == sf::Keyboard::Key::Escape)
        leaveGame();
    else if (_session.isWaiting() && (key == sf::Keyboard::Key::Enter || key == sf::Keyboard::Key::Space))
        _session.setReady(true);
}

void App::updateMenu(float dt)
{
    _menu.update(dt);
    switch (_menu.getAndResetAction()) {
    case ui::MenuAction::Exit:
        _window.close();
        break;
    case ui::MenuAction::Campaign:
        _campaign.open(static_cast<std::size_t>(_menu.getConfig().campaignUnlocked));
        _screen = Screen::Campaign;
        break;
    case ui::MenuAction::Infinite: {
        server::ServerSettings settings;
        settings.mode = server::Game::GameMode::Infinite;
        settings.difficulty = difficultyOf(_menu.getConfig().difficulty);
        settings.noLobby = true;
        _returnTo = Screen::Menu;
        host(settings);
        break;
    }
    case ui::MenuAction::Host:
        hostFromMenu();
        break;
    case ui::MenuAction::Editor:
        _editor.open();
        _screen = Screen::Editor;
        break;
    case ui::MenuAction::Connect:
        try {
            _returnTo = Screen::Menu;
            connect(_menu.getConfig().joinIp, static_cast<std::uint16_t>(std::stoul(_menu.getConfig().joinPort)));
        } catch (const std::exception &e) {
            _menu.setStatusMessage("Connection failed: " + std::string(e.what()), sf::Color::Red);
        }
        break;
    default:
        break;
    }
}

void App::hostFromMenu()
{
    const auto &config = _menu.getConfig();
    server::ServerSettings settings;
    settings.mode = config.gameMode == 0 ? server::Game::GameMode::Campaign : server::Game::GameMode::Infinite;
    settings.difficulty = difficultyOf(config.difficulty);
    settings.startLevel = static_cast<std::size_t>(config.mission) * ui::CampaignMap::LevelsPerWorld;
    _returnTo = Screen::Menu;
    host(settings, config.hostPort);
}

void App::updateCampaign(float dt)
{
    _campaign.update(dt);
    if (_campaign.takeBack())
        _screen = Screen::Menu;
    if (const auto choice = _campaign.takeChoice()) {
        server::ServerSettings settings;
        settings.startLevel = choice->level;
        settings.difficulty = difficultyOf(choice->difficulty);
        settings.noLobby = true;
        _returnTo = Screen::Campaign;
        host(settings);
    }
}

// The editor's Test plays the saved level on a local server; Escape comes back to the editor.
void App::updateEditor(float dt)
{
    _editor.update(dt);
    if (_editor.takeBack())
        _screen = Screen::Menu;
    if (const auto level = _editor.takeTest()) {
        server::ServerSettings settings;
        settings.level = *level;
        settings.noLobby = true;
        _returnTo = Screen::Editor;
        host(settings);
    }
}

void App::updateGame(float dt)
{
    _background.setWorld(static_cast<std::size_t>(_session.level()) / ui::CampaignMap::LevelsPerWorld);
    _session.update(dt, _network.connected());
    _session.setHighContrast(_menu.getConfig().highContrast);
    _session.sendInput(_network, _menu.getConfig(), dt, _window.hasFocus());
    _session.receive(_network);
    if (_returnTo == Screen::Campaign)
        recordCampaignProgress();
    checkConnection();
}

// Every level reached is unlocked on the map; winning the campaign unlocks them all.
void App::recordCampaignProgress()
{
    const int reached = _session.won() ? static_cast<int>(_campaign.levelCount()) - 1 : _session.level();
    if (reached > _menu.getConfig().campaignUnlocked)
        _menu.unlockLevel(reached);
}

// A refused, unreachable or lost server sends the player back, with the reason.
void App::checkConnection()
{
    switch (_network.state()) {
    case ConnectionState::Rejected:
        leaveGame("Connection rejected: " + _session.rejectionReason() + ".");
        break;
    case ConnectionState::TimedOut:
        leaveGame("Could not reach the server. Check the address and that the server is running.");
        break;
    case ConnectionState::Lost:
        leaveGame("Connection to the server lost.");
        break;
    default:
        break;
    }
}

void App::leaveGame(const std::string &message)
{
    _network.disconnect();
    _server.stop();
    _session.reset();
    if (!message.empty())
        _menu.setStatusMessage(message, sf::Color::Red);
    if (_returnTo == Screen::Campaign)
        _campaign.open(static_cast<std::size_t>(_menu.getConfig().campaignUnlocked));
    _screen = _returnTo;
}

// Bottom left, away from the HUD.
void App::drawStats()
{
    sf::Text text(_resources.getFont("default"), statsLine(_network.stats()), 22);
    text.setFillColor(sf::Color::White);
    text.setOutlineColor(sf::Color::Black);
    text.setOutlineThickness(2.f);
    text.setPosition({20.f, WorldHeight - 45.f});
    _window.draw(text);
}

// Everything is drawn in world coordinates (1920x1080); the view scales them to the window.
void App::draw(float dt)
{
    _window.clear(sf::Color(10, 10, 25));
    if (_screen == Screen::Playing) {
        _window.setView(_session.view());
        if (!_menu.getConfig().highContrast) // plain dark background in high contrast
            _background.draw(_window);
        _session.draw(_window, _time, dt, _hasAssets);
        if (_showStats)
            drawStats();
    } else {
        _window.setView(sf::View(sf::FloatRect({0.f, 0.f}, {WorldWidth, WorldHeight})));
        // The map shows the world of the selected level; the menu shows each world in turn.
        if (_screen == Screen::Editor) {
            _editor.draw(_window, _time); // with the background of the world chosen in the editor
        } else {
            _background.setWorld(_screen == Screen::Campaign ? _campaign.world()
                                                             : static_cast<std::size_t>(_time / 12.f));
            _background.draw(_window);
            if (_screen == Screen::Campaign)
                _campaign.draw(_window);
            else
                _menu.draw(_window);
        }
    }
    _window.display();
}

} // namespace rtype::client
