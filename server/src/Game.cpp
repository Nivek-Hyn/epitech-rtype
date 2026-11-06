// The game itself: its creation, one step of the simulation, and what is sent to the clients.

#include "Game.hpp"
#include "GameConstants.hpp"
#include "common/Components.hpp"
#include "engine/systems/Movement.hpp"
#include "MovementSystem.hpp"
#include "EnemyShootSystem.hpp"
#include <algorithm>
#include <utility>
#include <vector>

namespace rtype::server {

namespace {

using NetworkedEntity = std::pair<engine::ecs::Entity, std::uint32_t>;

constexpr float EnemyWidth = 66.f;

bool isOutOfScreen(const rtype::components::Kind &kind, const rtype::components::Position &position)
{
    switch (kind.type) {
    case protocol::EntityType::PlayerMissile:
    case protocol::EntityType::EnemyMissile:
        return position.x > WorldWidth || position.x < 0.f || position.y > WorldHeight || position.y < 0.f;
    case protocol::EntityType::Enemy:
        return position.x < -EnemyWidth || position.y > WorldHeight;
    case protocol::EntityType::Obstacle:
        return position.x < -ObstacleSize;
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
    loadEntitiesConfig("assets/config/entities.json");
    loadRules("assets/config/game_rules.json");

    _campaignLevels = _rules.campaignLevels;
    _currentLevelIndex = 0;
    if (!_campaignLevels.empty()) {
        try {
            loadCurrentLevel();
        } catch (...) {
            // Level file might not be found in some test environments
        }
    }
}

void Game::update(float dt)
{
    updatePlayerLives(dt);
    updateWaves(dt);
    updateEnemyMovements(_registry, dt);
    updateEnemyShooting(_registry, [this](protocol::EntityType type, std::uint8_t variant, float x, float y) {
        return spawnNetworked(type, variant, x, y);
    }, dt);
    _registry.update(dt);
    keepPlayersOnScreen();
    handleCollisions();
    updateShootCooldowns(dt);
    updateLevelProgression(dt);
    removeEntitiesOutOfScreen();
    _tick++;
}

void Game::keepPlayersOnScreen()
{
    _registry.each<rtype::components::Position, rtype::components::Kind>(
        [](engine::ecs::Entity /*entity*/, rtype::components::Position &position, const rtype::components::Kind &kind) {
            if (kind.type != protocol::EntityType::Player)
                return;
            position.x = std::clamp(position.x, 0.f, WorldWidth - PlayerWidth);
            position.y = std::clamp(position.y, 0.f, WorldHeight - PlayerHeight);
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
        [&outOfScreen](engine::ecs::Entity entity, const rtype::components::Position &position,
                       const rtype::components::Kind &kind, const rtype::components::NetworkId &networkId) {
            if (isOutOfScreen(kind, position))
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
                kind.type == protocol::EntityType::Obstacle)
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

protocol::Snapshot Game::snapshot()
{
    protocol::Snapshot snap;
    snap.tick = _tick;
    snap.state = _state;
    snap.score = _score;
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
