// The game itself: its creation, one step of the simulation, and what is sent to the clients.

#include "Game.hpp"
#include "EnemyShootSystem.hpp"
#include "GameComponents.hpp"
#include "MovementSystem.hpp"
#include "engine/systems/Movement.hpp"
#include <algorithm>
#include <iostream>
#include <utility>
#include <vector>

namespace rtype::server {

namespace {

using NetworkedEntity = std::pair<engine::ecs::Entity, std::uint32_t>;

bool isOutOfScreen(const rtype::components::Kind &kind, const rtype::components::Position &position, float width)
{
    switch (kind.type) {
    case protocol::EntityType::PlayerMissile:
    case protocol::EntityType::EnemyMissile:
        return position.x > WorldWidth || position.x < 0.f || position.y > WorldHeight || position.y < 0.f;
    case protocol::EntityType::Enemy:
        return position.x < -width || position.y > WorldHeight;
    case protocol::EntityType::Obstacle:
    case protocol::EntityType::Bonus:
        return position.x < -width;
    default:
        return false;
    }
}

} // namespace

Game::Game()
{
    _players.fill(engine::ecs::NullEntity);
    _registry.addSystem(engine::systems::movement<rtype::components::Position, rtype::components::Velocity>);

    applyDefaultRules();
    registerGameComponents(_prefabs);
    loadEntitiesConfig("assets/config/entities.json");
    loadRules("assets/config/game_rules.json");
    _arsenal = loadArsenal("assets/config/weapons.json");
    _weapons.fill(_arsenal.defaultWeapon);

    _campaignLevels = _rules.campaignLevels;
    _currentLevelIndex = 0;
    if (!_campaignLevels.empty()) {
        try {
            loadCurrentLevel();
        } catch (const std::exception &error) {
            // Not fatal here: tests load their own levels. A server checks every level before starting.
            std::cerr << "Warning: " << error.what() << '\n';
        }
    }
}

void Game::update(float dt)
{
    updateLobby(dt);
    updatePlayerLives(dt);
    updateWaves(dt);
    updateEnemyMovements(_registry, dt);
    updateEnemyShooting(
        _registry,
        [this](protocol::EntityType type, std::uint8_t variant, float x, float y) {
            return spawnShot(type, variant, x, y);
        },
        dt);
    _registry.update(dt);
    keepPlayersOnScreen();
    handleCollisions();
    collectBonuses();
    updateShootCooldowns(dt);
    updateLevelProgression(dt);
    removeEntitiesOutOfScreen();
    _tick++;
}

void Game::keepPlayersOnScreen()
{
    _registry.each<rtype::components::Position, rtype::components::Kind>(
        [this](engine::ecs::Entity entity, rtype::components::Position &position, const rtype::components::Kind &kind) {
            if (kind.type != protocol::EntityType::Player)
                return;
            const engine::physics::Aabb box = hitArea(entity);
            position.x = std::clamp(position.x, 0.f, WorldWidth - box.width);
            position.y = std::clamp(position.y, 0.f, WorldHeight - box.height);
        });
}

void Game::updateShootCooldowns(float dt)
{
    for (float &cooldown : _shootCooldowns)
        cooldown = std::max(0.f, cooldown - dt);
}

void Game::removeEntitiesOutOfScreen()
{
    std::vector<NetworkedEntity> outOfScreen;
    _registry.each<rtype::components::Position, rtype::components::Kind, rtype::components::NetworkId>(
        [this, &outOfScreen](engine::ecs::Entity entity, const rtype::components::Position &position,
                             const rtype::components::Kind &kind, const rtype::components::NetworkId &networkId) {
            if (isOutOfScreen(kind, position, hitArea(entity).width))
                outOfScreen.emplace_back(entity, networkId.id);
        });
    removeAsOutOfBounds(outOfScreen);
}

void Game::clearEnemies()
{
    std::vector<NetworkedEntity> toRemove;
    _registry.each<rtype::components::Kind, rtype::components::NetworkId>(
        [&toRemove](engine::ecs::Entity entity, const rtype::components::Kind &kind,
                    const rtype::components::NetworkId &networkId) {
            if (kind.type == protocol::EntityType::Enemy || kind.type == protocol::EntityType::EnemyMissile ||
                kind.type == protocol::EntityType::Obstacle || kind.type == protocol::EntityType::Bonus)
                toRemove.emplace_back(entity, networkId.id);
        });
    removeAsOutOfBounds(toRemove);
}

void Game::removeAsOutOfBounds(const std::vector<std::pair<engine::ecs::Entity, std::uint32_t>> &entities)
{
    for (const auto &[entity, networkId] : entities) {
        _registry.kill(entity);
        _events.push_back(protocol::EntityDestroyed{networkId, protocol::DestroyReason::OutOfBounds});
    }
}

engine::ecs::Entity Game::spawnNetworked(protocol::EntityType type, std::uint8_t variant, float x, float y)
{
    engine::ecs::Entity entity = _registry.spawn();
    _registry.emplace<rtype::components::NetworkId>(entity, _nextNetId++);
    _registry.emplace<rtype::components::Kind>(entity, type, variant);
    _registry.emplace<rtype::components::Position>(entity, x, y);
    _registry.emplace<rtype::components::Velocity>(entity, 0.0f, 0.0f);
    return entity;
}

// The prefab, made visible to the clients at the given position. Nothing is created for an unknown name.
std::optional<engine::ecs::Entity> Game::spawnPrefab(const std::string &name, float x, float y)
{
    if (!_prefabs.contains(name))
        return std::nullopt;
    const engine::ecs::Entity entity = _prefabs.spawn(_registry, name);
    _registry.emplace<rtype::components::NetworkId>(entity, _nextNetId++);
    _registry.emplace<rtype::components::Position>(entity, x, y);
    if (!_registry.has<rtype::components::Kind>(entity))
        _registry.emplace<rtype::components::Kind>(entity);
    if (!_registry.has<rtype::components::Velocity>(entity))
        _registry.emplace<rtype::components::Velocity>(entity);
    return entity;
}

// A missile from the prefab of its type, with the sprite variant chosen by the shooter.
engine::ecs::Entity Game::spawnShot(protocol::EntityType type, std::uint8_t variant, float x, float y)
{
    const char *prefab = type == protocol::EntityType::PlayerMissile ? "player_missile" : "enemy_missile";
    const auto shot = spawnPrefab(prefab, x, y);
    if (!shot)
        return spawnNetworked(type, variant, x, y);
    _registry.get<rtype::components::Kind>(*shot)->variant = variant;
    return *shot;
}

// Where the entity can be hit this tick, from its Position and Hitbox. Empty without a Hitbox.
engine::physics::Aabb Game::hitArea(engine::ecs::Entity entity)
{
    const auto *position = _registry.get<rtype::components::Position>(entity);
    const auto *hitbox = _registry.get<rtype::components::Hitbox>(entity);
    if (!position || !hitbox)
        return {};
    return {position->x, position->y + hitbox->offsetY, hitbox->width, hitbox->height};
}

protocol::Snapshot Game::snapshot()
{
    protocol::Snapshot snap;
    snap.tick = _tick;
    snap.state = _state;
    snap.score = _score;
    snap.level = static_cast<std::uint8_t>(std::min<std::size_t>(_currentLevelIndex, 255));
    snap.wave = static_cast<std::uint8_t>(std::count_if(_currentLevel.waves.begin(), _currentLevel.waves.end(),
                                                        [](const WaveConfig &wave) { return wave.isSpawned; }));
    for (std::size_t player = 0; player < _lives.size(); player++)
        snap.lives[player] = static_cast<std::uint8_t>(std::clamp(_lives[player], 0, 255));

    _registry.each<rtype::components::Position, rtype::components::Kind, rtype::components::NetworkId>(
        [&snap](engine::ecs::Entity /*entity*/, const rtype::components::Position &position,
                const rtype::components::Kind &kind, const rtype::components::NetworkId &networkId) {
            snap.entities.push_back({networkId.id, kind.type, kind.variant, position.x, position.y});
        });
    return snap;
}

std::vector<protocol::Message> Game::takeEvents()
{
    std::vector<protocol::Message> events;
    events.swap(_events);
    return events;
}

} // namespace rtype::server
