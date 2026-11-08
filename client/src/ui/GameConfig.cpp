#include "GameConfig.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

namespace rtype::client::ui {

std::string GameConfig::keyToString(sf::Keyboard::Key key)
{
    if (key >= sf::Keyboard::Key::A && key <= sf::Keyboard::Key::Z) {
        return std::string(1,
                           static_cast<char>('A' + (static_cast<int>(key) - static_cast<int>(sf::Keyboard::Key::A))));
    }
    if (key >= sf::Keyboard::Key::Num0 && key <= sf::Keyboard::Key::Num9) {
        return std::string(
            1, static_cast<char>('0' + (static_cast<int>(key) - static_cast<int>(sf::Keyboard::Key::Num0))));
    }
    if (key >= sf::Keyboard::Key::Numpad0 && key <= sf::Keyboard::Key::Numpad9) {
        return "Num" + std::string(1, static_cast<char>('0' + (static_cast<int>(key) -
                                                               static_cast<int>(sf::Keyboard::Key::Numpad0))));
    }

    switch (key) {
    case sf::Keyboard::Key::Up:
        return "Up";
    case sf::Keyboard::Key::Down:
        return "Down";
    case sf::Keyboard::Key::Left:
        return "Left";
    case sf::Keyboard::Key::Right:
        return "Right";
    case sf::Keyboard::Key::Space:
        return "Space";
    case sf::Keyboard::Key::Enter:
        return "Enter";
    case sf::Keyboard::Key::Escape:
        return "Escape";
    case sf::Keyboard::Key::Tab:
        return "Tab";
    case sf::Keyboard::Key::Backspace:
        return "Backspace";
    case sf::Keyboard::Key::LShift:
        return "LShift";
    case sf::Keyboard::Key::RShift:
        return "RShift";
    case sf::Keyboard::Key::LControl:
        return "LCtrl";
    case sf::Keyboard::Key::RControl:
        return "RCtrl";
    case sf::Keyboard::Key::LAlt:
        return "LAlt";
    case sf::Keyboard::Key::RAlt:
        return "RAlt";
    default:
        return "Unknown";
    }
}

sf::Keyboard::Key GameConfig::stringToKey(const std::string &str, sf::Keyboard::Key defaultKey)
{
    if (str.empty()) {
        return defaultKey;
    }

    std::string upper = str;
    for (char &c : upper) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    if (upper.size() == 1) {
        char c = upper[0];
        if (c >= 'A' && c <= 'Z') {
            return static_cast<sf::Keyboard::Key>(static_cast<int>(sf::Keyboard::Key::A) + (c - 'A'));
        }
        if (c >= '0' && c <= '9') {
            return static_cast<sf::Keyboard::Key>(static_cast<int>(sf::Keyboard::Key::Num0) + (c - '0'));
        }
    }

    if (upper == "UP" || upper == "ARROWUP")
        return sf::Keyboard::Key::Up;
    if (upper == "DOWN" || upper == "ARROWDOWN")
        return sf::Keyboard::Key::Down;
    if (upper == "LEFT" || upper == "ARROWLEFT")
        return sf::Keyboard::Key::Left;
    if (upper == "RIGHT" || upper == "ARROWRIGHT")
        return sf::Keyboard::Key::Right;
    if (upper == "SPACE" || upper == "SPACEBAR")
        return sf::Keyboard::Key::Space;
    if (upper == "ENTER" || upper == "RETURN")
        return sf::Keyboard::Key::Enter;
    if (upper == "ESCAPE" || upper == "ESC")
        return sf::Keyboard::Key::Escape;
    if (upper == "TAB")
        return sf::Keyboard::Key::Tab;
    if (upper == "BACKSPACE")
        return sf::Keyboard::Key::Backspace;
    if (upper == "LSHIFT" || upper == "SHIFT")
        return sf::Keyboard::Key::LShift;
    if (upper == "RSHIFT")
        return sf::Keyboard::Key::RShift;
    if (upper == "LCTRL" || upper == "LCONTROL" || upper == "CTRL")
        return sf::Keyboard::Key::LControl;
    if (upper == "RCTRL" || upper == "RCONTROL")
        return sf::Keyboard::Key::RControl;
    if (upper == "LALT" || upper == "ALT")
        return sf::Keyboard::Key::LAlt;
    if (upper == "RALT")
        return sf::Keyboard::Key::RAlt;

    if (upper.rfind("NUM", 0) == 0 && upper.size() == 4 && std::isdigit(upper[3])) {
        int idx = upper[3] - '0';
        return static_cast<sf::Keyboard::Key>(static_cast<int>(sf::Keyboard::Key::Numpad0) + idx);
    }

    return defaultKey;
}

void GameConfig::applyAzerty()
{
    keyUp = sf::Keyboard::Key::Z;
    keyDown = sf::Keyboard::Key::S;
    keyLeft = sf::Keyboard::Key::Q;
    keyRight = sf::Keyboard::Key::D;
    keyShoot = sf::Keyboard::Key::Space;
}

void GameConfig::applyQwerty()
{
    keyUp = sf::Keyboard::Key::W;
    keyDown = sf::Keyboard::Key::S;
    keyLeft = sf::Keyboard::Key::A;
    keyRight = sf::Keyboard::Key::D;
    keyShoot = sf::Keyboard::Key::Space;
}

void GameConfig::resetToDefaults()
{
    applyAzerty();
}

std::string GameConfig::getConflictAction(sf::Keyboard::Key key, const std::string &excludeAction) const
{
    if (key == sf::Keyboard::Key::Unknown) {
        return "";
    }
    if (excludeAction != "Up" && keyUp == key)
        return "Up";
    if (excludeAction != "Down" && keyDown == key)
        return "Down";
    if (excludeAction != "Left" && keyLeft == key)
        return "Left";
    if (excludeAction != "Right" && keyRight == key)
        return "Right";
    if (excludeAction != "Shoot" && keyShoot == key)
        return "Shoot";
    return "";
}

GameConfig GameConfig::loadFromFile(const std::string &path)
{
    GameConfig config;
    std::ifstream file(path);
    if (!file.is_open()) {
        return config;
    }

    try {
        nlohmann::json j;
        file >> j;

        if (j.contains("sfx_volume") && j["sfx_volume"].is_number()) {
            config.sfxVolume = std::clamp(j["sfx_volume"].get<float>(), 0.f, 100.f);
        } else if (j.contains("sfxVolume") && j["sfxVolume"].is_number()) {
            config.sfxVolume = std::clamp(j["sfxVolume"].get<float>(), 0.f, 100.f);
        }

        if (j.contains("music_volume") && j["music_volume"].is_number()) {
            config.musicVolume = std::clamp(j["music_volume"].get<float>(), 0.f, 100.f);
        } else if (j.contains("musicVolume") && j["musicVolume"].is_number()) {
            config.musicVolume = std::clamp(j["musicVolume"].get<float>(), 0.f, 100.f);
        }

        config.autoFire = j.value("auto_fire", false);
        config.highContrast = j.value("high_contrast", false);
        config.campaignUnlocked = std::max(0, j.value("campaign_unlocked", 0));
        if (j.contains("join_ip") && j["join_ip"].is_string()) {
            config.joinIp = j["join_ip"].get<std::string>();
        }
        if (j.contains("join_port") && j["join_port"].is_string()) {
            config.joinPort = j["join_port"].get<std::string>();
        }
        if (j.contains("host_port") && j["host_port"].is_string()) {
            config.hostPort = j["host_port"].get<std::string>();
        }

        if (j.contains("key_up") && j["key_up"].is_string()) {
            config.keyUp = stringToKey(j["key_up"].get<std::string>(), sf::Keyboard::Key::Unknown);
        }
        if (j.contains("key_down") && j["key_down"].is_string()) {
            config.keyDown = stringToKey(j["key_down"].get<std::string>(), sf::Keyboard::Key::Unknown);
        }
        if (j.contains("key_left") && j["key_left"].is_string()) {
            config.keyLeft = stringToKey(j["key_left"].get<std::string>(), sf::Keyboard::Key::Unknown);
        }
        if (j.contains("key_right") && j["key_right"].is_string()) {
            config.keyRight = stringToKey(j["key_right"].get<std::string>(), sf::Keyboard::Key::Unknown);
        }
        if (j.contains("key_shoot") && j["key_shoot"].is_string()) {
            config.keyShoot = stringToKey(j["key_shoot"].get<std::string>(), sf::Keyboard::Key::Unknown);
        }

        std::vector<sf::Keyboard::Key> keys = {config.keyUp, config.keyDown, config.keyLeft, config.keyRight,
                                               config.keyShoot};
        bool hasInvalidOrDuplicate = false;
        for (std::size_t i = 0; i < keys.size(); ++i) {
            if (keys[i] == sf::Keyboard::Key::Unknown) {
                hasInvalidOrDuplicate = true;
                break;
            }
            for (std::size_t k = i + 1; k < keys.size(); ++k) {
                if (keys[i] == keys[k]) {
                    hasInvalidOrDuplicate = true;
                    break;
                }
            }
        }

        if (hasInvalidOrDuplicate) {
            std::cerr << "[Config] Warning: Keybinding conflict or invalid key detected in " << path
                      << ". Resetting controls to defaults.\n";
            config.resetToDefaults();
        }

    } catch (const std::exception &e) {
        std::cerr << "[Config] Warning: Error parsing " << path << ": " << e.what() << '\n';
        config.resetToDefaults();
    }

    return config;
}

bool GameConfig::saveToFile(const std::string &path) const
{
    try {
        nlohmann::json j;
        j["sfx_volume"] = sfxVolume;
        j["music_volume"] = musicVolume;
        j["auto_fire"] = autoFire;
        j["high_contrast"] = highContrast;
        j["campaign_unlocked"] = campaignUnlocked;
        j["host_port"] = hostPort;
        j["join_ip"] = joinIp;
        j["join_port"] = joinPort;
        j["key_up"] = keyToString(keyUp);
        j["key_down"] = keyToString(keyDown);
        j["key_left"] = keyToString(keyLeft);
        j["key_right"] = keyToString(keyRight);
        j["key_shoot"] = keyToString(keyShoot);

        std::ofstream file(path);
        if (!file.is_open()) {
            std::cerr << "[Config] Warning: Unable to open " << path << " for writing\n";
            return false;
        }
        file << j.dump(4) << '\n';
        return true;
    } catch (const std::exception &e) {
        std::cerr << "[Config] Warning: Failed to save config to " << path << ": " << e.what() << '\n';
        return false;
    }
}

} // namespace rtype::client::ui
