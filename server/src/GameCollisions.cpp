// Collisions: every case of a shot or a ship touching something.

#include "Game.hpp"
#include "GameConstants.hpp"
#include "common/Components.hpp"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace rtype::server {

namespace {

using Box = engine::physics::Aabb;
using BoxedEntity = std::pair<engine::ecs::Entity, Box>;

constexpr float MissileWidth = 36.f;
constexpr float MissileHeight = 24.f;

Box playerBoxAt(const rtype::components::Position &position)
{
    return {position.x, position.y, PlayerWidth, PlayerHeight};
}

Box missileBoxAt(const rtype::components::Position &position)
{
    return {position.x, position.y, MissileWidth, MissileHeight};
}

bool overlapsAny(const Box &box, const std::vector<BoxedEntity> &others)
{
    return std::any_of(others.begin(), others.end(),
                       [&box](const BoxedEntity &other) { return engine::physics::overlaps(box, other.second); });
}

} // namespace

// What a collision pass knows: the hit areas of this tick, and what must be destroyed at the end of it.
struct Game::CollisionFrame {
    std::vector<BoxedEntity> enemies;
    std::vector<BoxedEntity> obstacles;
    std::vector<bool> enemyHit;
    std::vector<bool> obstacleHit;
    std::vector<engine::ecs::Entity> destroyed;
};

void Game::handleCollisions()
{
    CollisionFrame frame = collectHitAreas();
    resolvePlayerMissiles(frame);
    resolveShipContacts(frame);
    resolveEnemyMissiles(frame);
    destroyEntities(frame.destroyed);
}

Game::CollisionFrame Game::collectHitAreas()
{
    using rtype::components::Kind;
    using rtype::components::Position;

    CollisionFrame frame;
    _registry.each<Position, Kind>([&frame](engine::ecs::Entity entity, const Position &position, const Kind &kind) {
        if (kind.type == protocol::EntityType::Enemy) {
            const auto box = rtype::components::enemyHitbox(kind.variant);
            frame.enemies.push_back({entity, {position.x, position.y + box.offsetY, box.width, box.height}});
        } else if (kind.type == protocol::EntityType::Obstacle) {
            frame.obstacles.push_back({entity, {position.x, position.y, ObstacleSize, ObstacleSize}});
        }
    });
    frame.enemyHit.assign(frame.enemies.size(), false);
    frame.obstacleHit.assign(frame.obstacles.size(), false);
    return frame;
}

// A player shot stops on the first thing it meets: a tile protects what is behind it.
void Game::resolvePlayerMissiles(CollisionFrame &frame)
{
    using rtype::components::Kind;
    using rtype::components::Position;

    _registry.each<Position, Kind>([&](engine::ecs::Entity missile, const Position &position, const Kind &kind) {
        if (kind.type != protocol::EntityType::PlayerMissile)
            return;
        const Box box = missileBoxAt(position);
        if (!missileHitsObstacle(frame, missile, box))
            missileHitsEnemy(frame, missile, box);
    });
}

bool Game::missileHitsObstacle(CollisionFrame &frame, engine::ecs::Entity missile, const engine::physics::Aabb &box)
{
    for (std::size_t i = 0; i < frame.obstacles.size(); i++) {
        if (frame.obstacleHit[i] || !engine::physics::overlaps(box, frame.obstacles[i].second))
            continue;
        frame.obstacleHit[i] = true;
        frame.destroyed.push_back(missile);
        auto *health = _registry.get<rtype::components::Health>(frame.obstacles[i].first);
        if (health && --health->hp <= 0)
            frame.destroyed.push_back(frame.obstacles[i].first);
        return true;
    }
    return false;
}

bool Game::missileHitsEnemy(CollisionFrame &frame, engine::ecs::Entity missile, const engine::physics::Aabb &box)
{
    for (std::size_t i = 0; i < frame.enemies.size(); i++) {
        if (frame.enemyHit[i] || !engine::physics::overlaps(box, frame.enemies[i].second))
            continue;
        frame.enemyHit[i] = true;
        frame.destroyed.push_back(missile);
        damageEnemy(frame, frame.enemies[i].first);
        return true;
    }
    return false;
}

void Game::damageEnemy(CollisionFrame &frame, engine::ecs::Entity enemy)
{
    if (auto *health = _registry.get<rtype::components::Health>(enemy)) {
        health->hp -= 1;
        if (health->hp > 0)
            return;
    }
    frame.destroyed.push_back(enemy);
    _score += pointsFor(enemy);
}

// The campaign has no score: points only exist in infinite mode.
std::uint32_t Game::pointsFor(engine::ecs::Entity enemy)
{
    if (_gameMode != GameMode::Infinite)
        return 0;
    const float multiplier = currentDifficulty().scoreMultiplier + completedLoops() * _rules.infinite.scoreMultiplierPerLoop;
    const auto *traits = _registry.get<rtype::components::EnemyTraits>(enemy);
    const std::uint32_t base = traits ? traits->score : 100;
    return static_cast<std::uint32_t>(std::round(static_cast<float>(base) * multiplier));
}

void Game::resolveShipContacts(CollisionFrame &frame)
{
    using rtype::components::Kind;
    using rtype::components::Position;

    _registry.each<Position, Kind>([&](engine::ecs::Entity ship, const Position &position, const Kind &kind) {
        if (kind.type != protocol::EntityType::Player || isInvulnerable(ship))
            return;
        const Box box = playerBoxAt(position);
        shipRamsEnemies(frame, ship, box);
        // A tile destroys the ship and stays in place.
        if (overlapsAny(box, frame.obstacles))
            frame.destroyed.push_back(ship);
    });
}

// Ramming kills the ship and the enemy, but a boss must be defeated by shooting it.
void Game::shipRamsEnemies(CollisionFrame &frame, engine::ecs::Entity ship, const engine::physics::Aabb &box)
{
    for (std::size_t i = 0; i < frame.enemies.size(); i++) {
        if (frame.enemyHit[i] || !engine::physics::overlaps(box, frame.enemies[i].second))
            continue;
        frame.destroyed.push_back(ship);
        const auto *traits = _registry.get<rtype::components::EnemyTraits>(frame.enemies[i].first);
        if (!traits || !traits->isBoss) {
            frame.enemyHit[i] = true;
            frame.destroyed.push_back(frame.enemies[i].first);
        }
    }
}

void Game::resolveEnemyMissiles(CollisionFrame &frame)
{
    using rtype::components::Kind;
    using rtype::components::Position;

    _registry.each<Position, Kind>([&](engine::ecs::Entity missile, const Position &position, const Kind &kind) {
        if (kind.type != protocol::EntityType::EnemyMissile)
            return;
        const Box box = missileBoxAt(position);
        // A tile stops enemy fire without being damaged, so players can take cover behind it.
        if (overlapsAny(box, frame.obstacles))
            frame.destroyed.push_back(missile);
        else
            missileHitsShips(frame, missile, box);
    });
}

void Game::missileHitsShips(CollisionFrame &frame, engine::ecs::Entity missile, const engine::physics::Aabb &box)
{
    using rtype::components::Kind;
    using rtype::components::Position;

    _registry.each<Position, Kind>([&](engine::ecs::Entity ship, const Position &position, const Kind &kind) {
        if (kind.type != protocol::EntityType::Player || isInvulnerable(ship))
            return;
        if (engine::physics::overlaps(box, playerBoxAt(position))) {
            frame.destroyed.push_back(missile);
            frame.destroyed.push_back(ship);
        }
    });
}

void Game::destroyEntities(const std::vector<engine::ecs::Entity> &entities)
{
    for (const engine::ecs::Entity entity : entities) {
        loseLifeIfShip(entity);
        if (const auto *networkId = _registry.get<rtype::components::NetworkId>(entity))
            _events.push_back(protocol::EntityDestroyed{networkId->id, protocol::DestroyReason::Killed});
        _registry.kill(entity);
    }
}

void Game::loseLifeIfShip(engine::ecs::Entity entity)
{
    for (std::size_t player = 0; player < _players.size(); player++) {
        if (_players[player] != entity)
            continue;
        _players[player] = engine::ecs::NullEntity;
        _shootCooldowns[player] = 0.f;
        _lives[player] = std::max(0, _lives[player] - 1);
        _respawnTimers[player] = _rules.respawnDelay;
    }
}

} // namespace rtype::server
