#pragma once

#include "ResourceManager.hpp"
#include "game/LocalServer.hpp"
#include "game/Session.hpp"
#include "net/AsioNetworkClient.hpp"
#include "render/WorldBackground.hpp"
#include "ui/CampaignMap.hpp"
#include "ui/MainMenu.hpp"

#include <SFML/Graphics.hpp>

#include <cstdint>
#include <string>

namespace rtype::client {

// The client application: the menu, the campaign map, then a game session. Escape goes back.
class App {
public:
    App(ResourceManager &resources, bool hasAssets);

    void connect(const std::string &host, std::uint16_t port);
    void run();

private:
    enum class Screen { Menu, Campaign, Playing };

    void host(server::ServerSettings settings, const std::string &port = "");
    void applyVolumes();
    void handleEvents();
    void handleGameKey(sf::Keyboard::Key key);
    void updateMenu(float dt);
    void hostFromMenu();
    void updateCampaign(float dt);
    void updateGame(float dt);
    void recordCampaignProgress();
    void checkConnection();
    void leaveGame(const std::string &message = "");
    void drawStats();
    void draw(float dt);

    sf::RenderWindow _window;
    ResourceManager &_resources;
    bool _hasAssets;
    WorldBackground _background;
    ui::MainMenu _menu;
    ui::CampaignMap _campaign;
    AsioNetworkClient _network;
    Session _session;
    LocalServer _server;
    Screen _screen = Screen::Menu;
    bool _campaignGame = false; // the game was started from the campaign map: back to it afterwards
    bool _showStats = false;    // network statistics, toggled with F3
    float _time = 0.f;
    float _lastSfxVolume = 0.f;
};

} // namespace rtype::client
