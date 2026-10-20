#pragma once

#include <SFML/Graphics/Texture.hpp>

#include <string>
#include <unordered_map>

namespace rtype::client {

/// @brief Central resource manager that owns and caches SFML textures.
class ResourceManager {
public:
    ResourceManager() = default;

    /// @brief Loads all default textures needed for the game.
    /// @return True if all assets were successfully loaded.
    bool loadDefaults();

    /// @brief Loads a texture from file and stores it with the given identifier.
    /// @param id Unique identifier for the texture.
    /// @param path Path to the image file.
    /// @return True if loaded successfully.
    bool loadTexture(const std::string &id, const std::string &path);

    /// @brief Retrieves a loaded texture by its identifier.
    /// @param id Identifier of the texture.
    /// @return Const reference to the SFML texture.
    /// @throws std::out_of_range if the texture is not found.
    const sf::Texture &getTexture(const std::string &id) const;

private:
    std::unordered_map<std::string, sf::Texture> _textures;
};

} // namespace rtype::client
