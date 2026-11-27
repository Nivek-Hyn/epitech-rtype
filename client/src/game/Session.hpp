#pragma once

#include "ResourceManager.hpp"
#include "common/Constants.hpp"
#include "common/Protocol.hpp"
#include "common/SnapshotAssembler.hpp"
#include "common/SnapshotInterpolator.hpp"
#include "net/INetworkClient.hpp"
#include "render/EntityRenderer.hpp"
#include "ui/GameConfig.hpp"

#include <SFML/Graphics.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rtype::client {

// What the client knows about the game it is playing: the snapshots received, shown slightly in the past and
// smoothed (SnapshotInterpolator), and the visual effects they produce (explosions, damage flash, screen shake,
// wave banner). The server decides everything else.
class Session {
public:
    static constexpr float DamageFlashDuration = 0.45f; // seconds
    static constexpr float WaveBannerDuration = 2.8f;

    explicit Session(ResourceManager &resources) : _resources(resources), _renderer(resources) {}

    // Forgets the previous game, when going back to the menu.
    void reset();
    // Advances the timers of the visual effects.
    void update(float dt, bool connected);
    // Sends the pressed actions to the server: nothing once the game has ended, and no action while the window
    // does not have the focus. With auto fire, Shoot is sent without holding the key.
    void sendInput(INetworkClient &network, const ui::GameConfig &config, float dt, bool focused);
    // High contrast: outlined enemy shots and HUD, softer flash, no screen shake.
    void setHighContrast(bool enabled) { _highContrast = enabled; }
    // Reads every message the network received, then handles the snapshots whose time has come.
    void receive(INetworkClient &network);
    // Why the server refused us, after a ConnectRejected.
    const std::string &rejectionReason() const { return _rejectionReason; }
    bool isWaiting() const { return _waiting; }
    int level() const { return _level; } // index of the level being played, from the snapshots
    bool won() const { return _gameOver && _gameWon; }
    bool isReady() const { return _isReady; }
    void setReady(bool ready = true) { _isReady = ready; }

    // The world view, shaken for a moment when the local ship is hit.
    sf::View view() const;
    void draw(sf::RenderWindow &window, float time, float dt, bool hasAssets);

private:
    void handle(const protocol::Message &message, bool &gotSnapshot);
    void onSnapshot(const protocol::Snapshot &snapshot);
    void updateGameState(const protocol::Snapshot &snapshot);
    std::optional<protocol::EntityState> lastSeen(std::uint32_t netId) const;
    void onDestroyed(const protocol::EntityDestroyed &event);
    void onPlayerLeft(const protocol::PlayerLeft &event);
    void addExplosion(const protocol::EntityState &entity, float duration);
    float tiltOf(const protocol::EntityState &entity, float dt);

    // SessionHud.cpp
    void drawConnecting(sf::RenderWindow &window) const;
    void drawHud(sf::RenderWindow &window) const;
    void drawScore(sf::RenderWindow &window) const;
    void drawBossAlert(sf::RenderWindow &window) const;
    void drawPlayers(sf::RenderWindow &window) const;
    void drawPlayerLine(sf::RenderWindow &window, std::uint8_t player, bool active, float y) const;
    void drawPanel(sf::RenderWindow &window, sf::Vector2f position, sf::Vector2f size) const;
    void drawHeart(sf::RenderWindow &window, sf::Vector2f center, sf::Color color) const;
    void drawDamageFlash(sf::RenderWindow &window) const;
    void drawWaveBanner(sf::RenderWindow &window) const;
    void drawWeaponBanner(sf::RenderWindow &window) const;
    void drawWaitingRoom(sf::RenderWindow &window) const;
    void drawPlayerLeft(sf::RenderWindow &window) const;
    void drawAutoFire(sf::RenderWindow &window) const;
    void drawShotMarkers(sf::RenderWindow &window) const;
    void outline(sf::Text &text) const;
    void drawEndScreen(sf::RenderWindow &window) const;
    sf::Text centeredText(const std::string &string, unsigned int size, sf::Color color, sf::Vector2f center) const;

    ResourceManager &_resources;
    EntityRenderer _renderer;

    protocol::SnapshotAssembler _world; // rebuilds the world of a tick from its (possibly split) snapshots
    protocol::SnapshotInterpolator _interpolator{InterpolationDelaySnapshots};
    std::vector<protocol::EntityState> _entities; // last snapshot handled: what the effects compare with
    std::vector<protocol::EntityState> _shown;    // interpolated entities drawn this frame
    std::string _rejectionReason = "the server refused the connection";
    std::vector<Explosion> _explosions;
    std::optional<std::uint8_t> _localPlayer;
    std::array<std::uint8_t, 4> _lives{};
    std::uint32_t _score = 0;
    bool _connected = false;
    bool _gameOver = false;
    bool _gameWon = false;
    bool _playerWasSpawned = false;
    bool _waiting = false; // the server is in its waiting room
    bool _isReady = false;
    bool _highContrast = false;
    bool _autoFire = false; // shown in the HUD

    float _damageFlash = 0.f;
    float _shakeTimer = 0.f;
    float _shakeIntensity = 0.f;
    float _shootSoundCooldown = 0.f;

    int _level = 0; // from the snapshots
    int _wave = 0;
    float _waveBanner = 0.f;
    std::vector<std::uint32_t> _destroyed; // destroyed by an event, hidden until the snapshots drop them
    std::string _leftMessage;              // "P2 LEFT THE GAME"
    float _leftTimer = 0.f;

    float _localTilt = 0.f;
    std::unordered_map<std::uint32_t, float> _lastY; // last height of each remote ship
    std::unordered_map<std::uint32_t, float> _tilts; // displayed tilt of each remote ship

    std::string _currentWeapon = "BLASTER";
    std::string _weaponBannerText;
    float _weaponBannerTimer = 0.f;
};

} // namespace rtype::client
