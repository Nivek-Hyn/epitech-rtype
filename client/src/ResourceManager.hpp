#pragma once

#include <SFML/Audio/Music.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rtype::client {

/// @brief Central resource manager that owns and caches SFML textures, fonts, and sounds.
class ResourceManager {
public:
    ResourceManager() = default;

    /// @brief Loads all default textures, fonts, and audio needed for the game.
    /// @return True if all assets were successfully loaded.
    bool loadDefaults();

    // Textures
    bool loadTexture(const std::string &id, const std::string &path);
    const sf::Texture &getTexture(const std::string &id) const;

    // Fonts
    bool loadFont(const std::string &id, const std::string &path);
    const sf::Font &getFont(const std::string &id) const;
    bool hasFont(const std::string &id) const;

    // Audio - Sound effects
    bool loadSound(const std::string &id, const std::string &path);
    const sf::SoundBuffer &getSound(const std::string &id) const;
    bool hasSound(const std::string &id) const;
    void playSound(const std::string &id, float volume = 100.f);
    void updateAudio();

    // Audio - Music
    bool playMusic(const std::string &path, float volume = 50.f, bool loop = true);
    void stopMusic();
    void setMusicVolume(float volume);
    void setSfxVolume(float volume);
    float getSfxVolume() const;
    float getMusicVolume() const;

private:
    std::unordered_map<std::string, sf::Texture> _textures;
    std::unordered_map<std::string, sf::Font> _fonts;
    std::unordered_map<std::string, sf::SoundBuffer> _soundBuffers;
    std::vector<std::unique_ptr<sf::Sound>> _activeSounds;
    sf::Music _music;
    float _sfxVolume{100.f};
    float _musicVolume{100.f};
};

} // namespace rtype::client
