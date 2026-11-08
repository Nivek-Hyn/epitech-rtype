#include "game/Session.hpp"

#include "common/Constants.hpp"

#include <SFML/Window/Keyboard.hpp>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <unordered_set>

namespace rtype::client {

namespace {

using protocol::EntityState;
using protocol::EntityType;

constexpr float ShakeDuration = 0.35f;
constexpr float TiltSpeed = 14.f; // tilt units per second
constexpr float ShootSoundDelay = 0.22f;

void countDown(float &timer, float dt)
{
    if (timer > 0.f)
        timer -= dt;
}

// Moves the displayed tilt toward the target, without jumping.
void approach(float &tilt, float target, float dt)
{
    if (tilt < target)
        tilt = std::min(target, tilt + dt * TiltSpeed);
    else if (tilt > target)
        tilt = std::max(target, tilt - dt * TiltSpeed);
}

bool pressed(sf::Keyboard::Key configured, sf::Keyboard::Key arrow)
{
    return sf::Keyboard::isKeyPressed(configured) || sf::Keyboard::isKeyPressed(arrow);
}

// Rough hit radius around an enemy sprite, to play the hit effect where a missile disappeared.
std::pair<float, float> hitRadiusAndOffset(std::uint8_t variant)
{
    switch (variant) {
    case 6:
        return {180.f, -60.f};
    case 5:
        return {130.f, -20.f};
    case 4:
        return {80.f, 0.f};
    default:
        return {65.f, 0.f};
    }
}

bool within(float dx, float dy, float radius)
{
    return dx * dx + dy * dy < radius * radius;
}

} // namespace

void Session::reset()
{
    _world.reset();
    _interpolator.reset();
    _entities.clear();
    _shown.clear();
    _rejectionReason = "the server refused the connection";
    _explosions.clear();
    _localPlayer.reset();
    _lives.fill(0);
    _score = 0;
    _connected = _gameOver = _gameWon = _playerWasSpawned = _waiting = false; // settings are kept
    _damageFlash = _shakeTimer = _waveBanner = _waveCooldown = _localTilt = 0.f;
    _wave = 1;
    _waveStarted = false;
    _lastEnemyCount = 0;
    _lastY.clear();
    _tilts.clear();
}

void Session::update(float dt, bool connected)
{
    countDown(_damageFlash, dt);
    countDown(_shakeTimer, dt);
    countDown(_waveBanner, dt);
    countDown(_waveCooldown, dt);
    for (auto &explosion : _explosions)
        explosion.timer += dt;
    std::erase_if(_explosions, [](const Explosion &explosion) { return explosion.timer >= explosion.maxDuration; });

    if (connected && !_connected) {
        std::cout << "Connected to server!\n";
        _connected = true;
    }
}

void Session::sendInput(INetworkClient &network, const ui::GameConfig &config, float dt, bool focused)
{
    using sf::Keyboard::Key;
    float targetTilt = 0.f;
    _autoFire = config.autoFire;
    if (!_gameOver && !focused) {
        if (network.connected())
            network.send(protocol::Input{0}); // keys of another window must not move the ship
    } else if (!_gameOver) {
        std::uint8_t flags = 0;
        if (pressed(config.keyUp, Key::Up)) {
            flags |= protocol::InputFlag::Up;
            targetTilt += 2.f;
        }
        if (pressed(config.keyDown, Key::Down)) {
            flags |= protocol::InputFlag::Down;
            targetTilt -= 2.f;
        }
        if (pressed(config.keyLeft, Key::Left))
            flags |= protocol::InputFlag::Left;
        if (pressed(config.keyRight, Key::Right))
            flags |= protocol::InputFlag::Right;

        _shootSoundCooldown -= dt;
        if (config.autoFire || sf::Keyboard::isKeyPressed(config.keyShoot)) {
            flags |= protocol::InputFlag::Shoot;
            if (_shootSoundCooldown <= 0.f) {
                _resources.playSound("shoot", 80.f);
                _shootSoundCooldown = ShootSoundDelay;
            }
        }
        if (network.connected())
            network.send(protocol::Input{flags});
    }
    approach(_localTilt, targetTilt, dt);
}

void Session::receive(INetworkClient &network)
{
    bool gotSnapshot = false;
    while (auto message = network.poll())
        handle(*message, gotSnapshot);

    // The packets of a split tick are complete only once the batch is drained: hand the world over once.
    const auto now = protocol::SnapshotInterpolator::Clock::now();
    if (gotSnapshot)
        _interpolator.push({_world.tick(), _world.state(), _world.score(), _world.entities(), _world.lives()}, now);
    // Each snapshot is handled when its time comes, so that sounds, explosions and HUD match what is drawn.
    while (const auto due = _interpolator.popDue(now))
        onSnapshot(*due);
}

void Session::handle(const protocol::Message &message, bool &gotSnapshot)
{
    if (const auto *accepted = std::get_if<protocol::ConnectAccepted>(&message)) {
        _localPlayer = accepted->playerId;
        // A new session: the server's ticks may start again from zero.
        _world.reset();
        _interpolator.reset();
        _entities.clear();
        gotSnapshot = false;
    } else if (const auto *snapshot = std::get_if<protocol::Snapshot>(&message)) {
        gotSnapshot = _world.add(*snapshot) || gotSnapshot;
    } else if (const auto *rejected = std::get_if<protocol::ConnectRejected>(&message)) {
        _rejectionReason = rejected->reason == protocol::RejectReason::ServerFull ? "the server is full"
                                                                                  : "the protocol versions differ";
        std::cerr << "Server rejected connection: " << _rejectionReason << ".\n";
    }
}

void Session::onSnapshot(const protocol::Snapshot &snapshot)
{
    _score = snapshot.score;
    _lives = snapshot.lives;
    updateGameState(snapshot);
    updateWaveCounter(snapshot);
    if (!_entities.empty())
        playDestroyEffects(snapshot);
    _entities = snapshot.entities;
}

// The server decides when the game is lost or won.
void Session::updateGameState(const protocol::Snapshot &snapshot)
{
    if (std::any_of(snapshot.entities.begin(), snapshot.entities.end(),
                    [](const EntityState &entity) { return entity.type == EntityType::Player; }))
        _playerWasSpawned = true;
    _waiting = snapshot.state == protocol::GameState::Waiting;
    if (snapshot.state == protocol::GameState::GameOver || snapshot.state == protocol::GameState::Finished) {
        _gameOver = true;
        _gameWon = snapshot.state == protocol::GameState::Finished;
    }
}

// A new wave is announced when enemies appear again after a calm moment.
void Session::updateWaveCounter(const protocol::Snapshot &snapshot)
{
    const int enemies =
        static_cast<int>(std::count_if(snapshot.entities.begin(), snapshot.entities.end(),
                                       [](const EntityState &e) { return e.type == EntityType::Enemy; }));
    if (_connected && !_waveStarted && snapshot.state == protocol::GameState::Playing) {
        _waveStarted = true;
        _wave = 1;
        _waveBanner = WaveBannerDuration;
        _waveCooldown = 6.0f;
    } else if (_waveStarted && _waveCooldown <= 0.f && _lastEnemyCount == 0 && enemies > 0) {
        _wave++;
        _waveBanner = WaveBannerDuration;
        _waveCooldown = 8.0f;
    }
    _lastEnemyCount = enemies;
}

// Entities of the previous snapshot that disappeared were destroyed: explosions, sounds, flash.
void Session::playDestroyEffects(const protocol::Snapshot &snapshot)
{
    std::unordered_set<std::uint32_t> alive;
    for (const auto &entity : snapshot.entities)
        alive.insert(entity.netId);
    std::vector<sf::Vector2f> ships;
    for (const auto &entity : _entities) {
        if (entity.type == EntityType::Player)
            ships.push_back({entity.x, entity.y});
    }
    for (const auto &entity : _entities) {
        if (!alive.contains(entity.netId))
            onDestroyed(entity, snapshot, ships);
    }
}

void Session::onDestroyed(const EntityState &entity, const protocol::Snapshot &snapshot,
                          const std::vector<sf::Vector2f> &ships)
{
    switch (entity.type) {
    case EntityType::Enemy:
    case EntityType::Obstacle:
        // Ignore what left the screen: only an entity destroyed on screen explodes.
        if (entity.x > 30.f && entity.x < WorldWidth + 100.f) {
            _resources.playSound("explosion", 90.f);
            addExplosion(entity, 0.35f);
        }
        break;
    case EntityType::Player:
        _resources.playSound("hit", 100.f);
        _resources.playSound("explosion", 100.f);
        addExplosion(entity, 0.45f);
        _damageFlash = DamageFlashDuration;
        _shakeTimer = ShakeDuration;
        _shakeIntensity = 15.f;
        break;
    case EntityType::EnemyMissile:
        onShipHit(entity, ships);
        break;
    case EntityType::PlayerMissile:
        onEnemyHit(entity, snapshot);
        break;
    case EntityType::Bonus:
        break;
    }
}

// An enemy shot that disappeared next to a ship hit it.
void Session::onShipHit(const EntityState &shot, const std::vector<sf::Vector2f> &ships)
{
    for (const auto &ship : ships) {
        if (within(shot.x - ship.x, shot.y - ship.y, 75.f)) {
            _resources.playSound("hit", 95.f);
            addExplosion(shot, 0.25f);
            _damageFlash = std::max(_damageFlash, 0.25f);
            _shakeTimer = std::max(_shakeTimer, 0.2f);
            _shakeIntensity = std::max(_shakeIntensity, 8.f);
            return;
        }
    }
}

// A player missile that disappeared next to an enemy hit it.
void Session::onEnemyHit(const EntityState &missile, const protocol::Snapshot &snapshot)
{
    for (const auto &enemy : snapshot.entities) {
        if (enemy.type != EntityType::Enemy)
            continue;
        const auto [radius, offsetY] = hitRadiusAndOffset(enemy.variant);
        if (within(missile.x - enemy.x, missile.y - (enemy.y + offsetY), radius)) {
            _resources.playSound("hit", 95.f);
            addExplosion(missile, 0.2f);
            return;
        }
    }
}

void Session::addExplosion(const EntityState &entity, float duration)
{
    _explosions.push_back({sf::Vector2f(entity.x, entity.y), 0.f, duration});
}

sf::View Session::view() const
{
    sf::View view(sf::FloatRect({0.f, 0.f}, {WorldWidth, WorldHeight}));
    if (_shakeTimer > 0.f && !_highContrast) {
        const float strength = (_shakeTimer / ShakeDuration) * _shakeIntensity;
        const float offsetX = (static_cast<float>(std::rand() % 200 - 100) / 100.f) * strength;
        const float offsetY = (static_cast<float>(std::rand() % 200 - 100) / 100.f) * strength;
        view.setCenter({WorldWidth / 2.f + offsetX, WorldHeight / 2.f + offsetY});
    }
    return view;
}

// The local ship tilts with the keys; a remote ship tilts with its vertical movement.
float Session::tiltOf(const EntityState &entity, float dt)
{
    if (_localPlayer == entity.variant)
        return _localTilt;
    float target = 0.f;
    if (const auto last = _lastY.find(entity.netId); last != _lastY.end()) {
        const float dy = entity.y - last->second;
        if (dy < -0.4f)
            target = 2.f;
        else if (dy > 0.4f)
            target = -2.f;
    }
    _lastY[entity.netId] = entity.y;
    float &tilt = _tilts[entity.netId];
    approach(tilt, target, dt);
    return tilt;
}

void Session::draw(sf::RenderWindow &window, float time, float dt, bool hasAssets)
{
    _shown = _interpolator.sample(protocol::SnapshotInterpolator::Clock::now());
    if (!_connected || _shown.empty())
        drawConnecting(window);
    if (hasAssets) {
        for (const auto &entity : _shown) {
            const float tilt = entity.type == EntityType::Player ? tiltOf(entity, dt) : 0.f;
            _renderer.draw(window, entity, time, tilt, _localPlayer);
        }
    }
    if (_highContrast)
        drawShotMarkers(window);
    for (const auto &explosion : _explosions)
        _renderer.drawExplosion(window, explosion);
    drawHud(window);
}

} // namespace rtype::client
