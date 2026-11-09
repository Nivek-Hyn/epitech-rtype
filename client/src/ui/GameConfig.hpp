#pragma once

#include <SFML/Window/Keyboard.hpp>
#include <string>

namespace rtype::client::ui {

struct GameConfig {
    int gameMode{0};
    int difficulty{1};
    int mission{0};
    std::string hostPort{"4242"};
    std::string joinIp{"127.0.0.1"};
    std::string joinPort{"4242"};
    float sfxVolume{100.f};
    float musicVolume{100.f};
    bool autoFire{false};     // shoot without holding the fire key (the server keeps its fire rate)
    bool highContrast{false}; // plain background, outlined shots and HUD, softer flashes
    int campaignUnlocked{0};  // last level playable on the campaign map, from 0

    sf::Keyboard::Key keyUp{sf::Keyboard::Key::Z};
    sf::Keyboard::Key keyDown{sf::Keyboard::Key::S};
    sf::Keyboard::Key keyLeft{sf::Keyboard::Key::Q};
    sf::Keyboard::Key keyRight{sf::Keyboard::Key::D};
    sf::Keyboard::Key keyShoot{sf::Keyboard::Key::Space};

    static std::string keyToString(sf::Keyboard::Key key);
    static sf::Keyboard::Key stringToKey(const std::string &str,
                                         sf::Keyboard::Key defaultKey = sf::Keyboard::Key::Unknown);

    void applyAzerty();
    void applyQwerty();
    void resetToDefaults();
    std::string getConflictAction(sf::Keyboard::Key key, const std::string &excludeAction = "") const;

    static GameConfig loadFromFile(const std::string &path = "config.json");
    bool saveToFile(const std::string &path = "config.json") const;
};

} // namespace rtype::client::ui
