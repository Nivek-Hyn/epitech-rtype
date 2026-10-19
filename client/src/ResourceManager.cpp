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

} // namespace rtype::client
