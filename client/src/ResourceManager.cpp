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
    loadTexture("enemy_standard", "assets/r-typesheet5.gif");
    loadTexture("enemy_patrol", "assets/r-typesheet27.gif");
    loadTexture("enemy_sine", "assets/r-typesheet12.gif");
    loadTexture("enemy_orbital_probe", "assets/r-typesheet12.gif");
    loadTexture("enemy_diver", "assets/r-typesheet8.gif");
    loadTexture("enemy_solar_relay", "assets/r-typesheet13.gif");
    loadTexture("enemy_rover", "assets/r-typesheet11.gif");
    loadTexture("enemy_walker", "assets/r-typesheet10.gif");
    loadTexture("enemy_interceptor", "assets/r-typesheet24.gif");
    loadTexture("enemy_tentacle", "assets/r-typesheet9.gif");
    loadTexture("enemy_boss_heavy", "assets/r-typesheet28.gif");
    loadTexture("enemy_boss", "assets/r-typesheet38.gif");
    loadTexture("explosion_boss", "assets/r-typesheet44.gif");
    loadTexture("obstacles", "assets/r-typesheet21.gif");
    if (!loadTexture("bullet", "assets/r-typesheet1.gif")) {
        std::cerr << "[ResourceManager] Failed to load assets/r-typesheet1.gif\n";
        success = false;
    }
    if (!loadTexture("enemy_bullet", "assets/r-typesheet3.gif")) {
        std::cerr << "[ResourceManager] Failed to load assets/r-typesheet3.gif\n";
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
    loadSound("shoot", "assets/audio/shoot.wav");
    loadSound("explosion", "assets/audio/explosion.wav");
    loadSound("hit", "assets/audio/hit.wav");
    loadSound("click", "assets/audio/click.wav");

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

bool ResourceManager::hasTexture(const std::string &id) const
{
    return _textures.find(id) != _textures.end();
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

bool ResourceManager::loadSound(const std::string &id, const std::string &path)
{
    sf::SoundBuffer buffer;
    if (!buffer.loadFromFile(path)) {
        std::cerr << "[ResourceManager] Warning: Failed to load sound " << path << '\n';
        return false;
    }
    _soundBuffers[id] = std::move(buffer);
    return true;
}

const sf::SoundBuffer &ResourceManager::getSound(const std::string &id) const
{
    const auto it = _soundBuffers.find(id);
    if (it == _soundBuffers.end())
        throw std::out_of_range("Sound buffer not found: " + id);
    return it->second;
}

bool ResourceManager::hasSound(const std::string &id) const
{
    return _soundBuffers.find(id) != _soundBuffers.end();
}

void ResourceManager::playSound(const std::string &id, float volume)
{
    if (_sfxVolume <= 0.01f)
        return;

    const auto it = _soundBuffers.find(id);
    if (it == _soundBuffers.end())
        return;

    updateAudio();

    if (_activeSounds.size() > 20) {
        _activeSounds.erase(_activeSounds.begin());
    }

    auto sound = std::make_unique<sf::Sound>(it->second);
    sound->setVolume(_sfxVolume * (volume / 100.f));
    sound->play();
    _activeSounds.push_back(std::move(sound));
}

void ResourceManager::updateAudio()
{
    std::erase_if(_activeSounds, [](const auto &soundPtr) {
        return !soundPtr || soundPtr->getStatus() == sf::Sound::Status::Stopped;
    });
}

bool ResourceManager::playMusic(const std::string &path, float volume, bool loop)
{
    if (!_music.openFromFile(path)) {
        std::cerr << "[ResourceManager] Warning: Failed to open music " << path << '\n';
        return false;
    }
    _musicVolume = volume;
    _music.setVolume(_musicVolume);
    _music.setLooping(loop);
    _music.play();
    return true;
}

void ResourceManager::stopMusic()
{
    _music.stop();
}

void ResourceManager::setMusicVolume(float volume)
{
    _musicVolume = volume;
    _music.setVolume(_musicVolume);
}

void ResourceManager::setSfxVolume(float volume)
{
    _sfxVolume = volume;
}

float ResourceManager::getSfxVolume() const
{
    return _sfxVolume;
}

float ResourceManager::getMusicVolume() const
{
    return _musicVolume;
}

} // namespace rtype::client
