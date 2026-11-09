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
    _connected = _gameOver = _gameWon = _playerWasSpawned = _waiting = _isReady = false; // settings are kept
    _damageFlash = _shakeTimer = _waveBanner = _leftTimer = _localTilt = 0.f;
    _level = _wave = 0;
    _destroyed.clear();
    _leftMessage.clear();
    _lastY.clear();
    _tilts.clear();
    _currentWeapon = "BLASTER";
    _weaponBannerText.clear();
    _weaponBannerTimer = 0.f;
}

void Session::update(float dt, bool connected)
{
    countDown(_damageFlash, dt);
    countDown(_shakeTimer, dt);
    countDown(_waveBanner, dt);
    countDown(_waveCooldown, dt);
    countDown(_weaponBannerTimer, dt);
    countDown(_leftTimer, dt);
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

        if (config.autoFire || sf::Keyboard::isKeyPressed(config.keyShoot)) {
            flags |= protocol::InputFlag::Shoot;
        }
        if (_waiting && (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter) ||
                         sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) || _isReady)) {
            _isReady = true;
            flags |= protocol::InputFlag::Ready;
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
        _interpolator.push(_world.snapshot(), now);
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
    } else if (const auto *destroyed = std::get_if<protocol::EntityDestroyed>(&message)) {
        onDestroyed(*destroyed);
    } else if (const auto *left = std::get_if<protocol::PlayerLeft>(&message)) {
        onPlayerLeft(*left);
    }
}

void Session::onSnapshot(const protocol::Snapshot &snapshot)
{
    _score = snapshot.score;
    _lives = snapshot.lives;
    _level = snapshot.level;
    if (snapshot.wave > _wave && snapshot.state == protocol::GameState::Playing)
        _waveBanner = WaveBannerDuration; // a new wave has come
    _wave = snapshot.wave;
    updateGameState(snapshot);
    updateWaveCounter(snapshot);
    if (!_entities.empty()) {
        playDestroyEffects(snapshot);
        playSpawnEffects(snapshot);
    }
    _entities = snapshot.entities;
    // A destroyed entity stays hidden until it is gone from the snapshots too.
    std::erase_if(_destroyed, [this](std::uint32_t netId) {
        return std::none_of(_entities.begin(), _entities.end(),
                            [netId](const EntityState &entity) { return entity.netId == netId; });
    });
}

// The server decides when the game is lost or won.
void Session::updateGameState(const protocol::Snapshot &snapshot)
{
    if (std::any_of(snapshot.entities.begin(), snapshot.entities.end(),
                    [](const EntityState &entity) { return entity.type == EntityType::Player; }))
        _playerWasSpawned = true;
    _waiting = snapshot.state == protocol::GameState::Waiting;
    // The end screen stays while the server shows it; a new game (waiting room) removes it.
    _gameOver = snapshot.state == protocol::GameState::GameOver || snapshot.state == protocol::GameState::Finished;
    _gameWon = snapshot.state == protocol::GameState::Finished;
}

// Where the entity is drawn now (or was in the last snapshot), to place its explosion.
std::optional<EntityState> Session::lastSeen(std::uint32_t netId) const
{
    for (const auto *list : {&_shown, &_entities}) {
        const auto found = std::find_if(list->begin(), list->end(),
                                        [netId](const EntityState &entity) { return entity.netId == netId; });
        if (found != list->end())
            return *found;
    }
    return std::nullopt;
}

void Session::playSpawnEffects(const protocol::Snapshot &snapshot)
{
    std::unordered_set<std::uint32_t> oldEntities;
    for (const auto &entity : _entities)
        oldEntities.insert(entity.netId);

    int newBlasterMissiles = 0;
    int newLaserMissiles = 0;

    for (const auto &entity : snapshot.entities) {
        if (!oldEntities.contains(entity.netId) && entity.type == protocol::EntityType::PlayerMissile) {
            if (entity.variant == 1)
                newLaserMissiles++;
            else
                newBlasterMissiles++;
        }
    }

    if (newLaserMissiles > 0) {
        // Laser fires slower (0.45s) with a deeper, heavy sound
        _resources.playSound("shoot", 95.f, 0.72f);
        _currentWeapon = "LASER";
    } else if (newBlasterMissiles > 0) {
        if (newBlasterMissiles >= 3) {
            _resources.playSound("shoot", 85.f, 1.15f);
            _currentWeapon = "SPREAD";
        } else {
            _resources.playSound("shoot", 80.f, 1.0f);
        }
    }
}

void Session::onDestroyed(const EntityState &entity, const protocol::Snapshot &snapshot,
                          const std::vector<sf::Vector2f> &ships)
// The server tells why each entity disappeared: only a destroyed one explodes.
void Session::onDestroyed(const protocol::EntityDestroyed &event)
{
    const std::optional<EntityState> entity = lastSeen(event.netId);
    if (!entity)
        return;
    _destroyed.push_back(event.netId);
    if (event.reason == protocol::DestroyReason::Collected)
        _resources.playSound("shoot", 60.f);
    if (event.reason != protocol::DestroyReason::Killed)
        return;
    switch (entity->type) {
    case EntityType::Enemy:
    case EntityType::Obstacle:
        _resources.playSound("explosion", 90.f);
        addExplosion(*entity, 0.35f);
        break;
    case EntityType::Player:
        _currentWeapon = "BLASTER";
        _resources.playSound("hit", 100.f);
        _resources.playSound("explosion", 100.f);
        addExplosion(*entity, 0.45f);
        _damageFlash = DamageFlashDuration;
        _shakeTimer = ShakeDuration;
        _shakeIntensity = 15.f;
        break;
    case EntityType::PlayerMissile:
    case EntityType::EnemyMissile: // a shot that hit something
        _resources.playSound("hit", 95.f);
        addExplosion(*entity, 0.2f);
        break;
    case EntityType::Bonus:
        if (entity.x > 30.f && entity.x < WorldWidth - 30.f) {
            _resources.playSound("hit", 90.f, 1.5f);
            if (entity.variant == 0) {
                _currentWeapon = "SPREAD";
                _weaponBannerText = "+ SPREAD SHOT (3-WAY) !";
                _weaponBannerTimer = 2.0f;
            } else if (entity.variant == 1) {
                _currentWeapon = "LASER";
                _weaponBannerText = "+ LASER BEAM (3x DAMAGE) !";
                _weaponBannerTimer = 2.0f;
            } else {
                _weaponBannerText = "+ EXTRA LIFE (+1) !";
                _weaponBannerTimer = 2.0f;
            }
        }
        break;
    }
}

// "P2 LEFT THE GAME" for a few seconds, so the other players know why a ship vanished.
void Session::onPlayerLeft(const protocol::PlayerLeft &event)
{
    _leftMessage = "P" + std::to_string(event.playerId + 1) +
                   (event.reason == protocol::LeaveReason::TimedOut ? " LOST THE CONNECTION" : " LEFT THE GAME");
    _leftTimer = 3.f;
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
            if (std::find(_destroyed.begin(), _destroyed.end(), entity.netId) != _destroyed.end())
                continue;
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
