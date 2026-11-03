#include "ResourceManager.hpp"

#include <iostream>
#include <stdexcept>

namespace rtype::client {

bool ResourceManager::loadDefaults()
{
    bool success = true;
    if (!loadTexture("player", "assets/r-typesheet42.gif")) {
        std::cerr << "[ResourceManager] Failed to load assets/r-typesheet42.gif\n";
        success = false;
    }
    if (!loadTexture("enemy", "assets/r-typesheet5.gif")) {
        std::cerr << "[ResourceManager] Failed to load assets/r-typesheet5.gif\n";
        success = false;
    }
    if (!loadTexture("bullet", "assets/r-typesheet1.gif")) {
        std::cerr << "[ResourceManager] Failed to load assets/r-typesheet1.gif\n";
        success = false;
    }

    if (!loadFont("default", "assets/font.ttf")) {
        if (!loadFont("default", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf") &&
            !loadFont("default", "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf") &&
            !loadFont("default", "/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf")) {
            std::cerr << "[ResourceManager] Warning: Failed to load default font.\n";
            success = false;
        }
    }

    return success;
}

bool ResourceManager::loadTexture(const std::string &id, const std::string &path)
{
    sf::Texture texture;
    if (!texture.loadFromFile(path))
        return false;
    _textures[id] = std::move(texture);
    return true;
}

const sf::Texture &ResourceManager::getTexture(const std::string &id) const
{
    const auto it = _textures.find(id);
    if (it == _textures.end())
        throw std::out_of_range("Texture not found: " + id);
    return it->second;
}

bool ResourceManager::loadFont(const std::string &id, const std::string &path)
{
    sf::Font font;
    if (!font.openFromFile(path))
        return false;
    _fonts[id] = std::move(font);
    return true;
}

const sf::Font &ResourceManager::getFont(const std::string &id) const
{
    const auto it = _fonts.find(id);
    if (it == _fonts.end())
        throw std::out_of_range("Font not found: " + id);
    return it->second;
}

bool ResourceManager::hasFont(const std::string &id) const
{
    return _fonts.find(id) != _fonts.end();
}

}
