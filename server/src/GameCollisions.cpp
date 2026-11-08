// Collisions: every case of a shot or a ship touching something.

#include "Game.hpp"
#include "common/Components.hpp"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace rtype::server {

namespace {

using Box = engine::physics::Aabb;
using BoxedEntity = std::pair<engine::ecs::Entity, Box>;

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
    std::vector<std::pair<float, float>> killedEnemies; // where a bonus may drop
};

void Game::handleCollisions()
{
    CollisionFrame frame = collectHitAreas();
    resolvePlayerMissiles(frame);
    resolveShipContacts(frame);
    resolveEnemyMissiles(frame);
    destroyEntities(frame.destroyed);
    dropBonuses(frame.killedEnemies);
}

Game::CollisionFrame Game::collectHitAreas()
{
    using rtype::components::Kind;
    using rtype::components::Position;

    CollisionFrame frame;
    _registry.each<Position, Kind>([this, &frame](engine::ecs::Entity entity, const Position &, const Kind &kind) {
        if (kind.type == protocol::EntityType::Enemy)
            frame.enemies.push_back({entity, hitArea(entity)});
        else if (kind.type == protocol::EntityType::Obstacle)
            frame.obstacles.push_back({entity, hitArea(entity)});
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

    _registry.each<Position, Kind>([&](engine::ecs::Entity missile, const Position &, const Kind &kind) {
        if (kind.type != protocol::EntityType::PlayerMissile)
            return;
        const Box box = hitArea(missile);
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
        damageEnemy(frame, frame.enemies[i], missile);
        return true;
    }
    return false;
}

// The missile removes its damage from the enemy's health.
void Game::damageEnemy(CollisionFrame &frame, const std::pair<engine::ecs::Entity, engine::physics::Aabb> &enemy,
                       engine::ecs::Entity missile)
{
    const auto *damage = _registry.get<rtype::components::MissileDamage>(missile);
    if (auto *health = _registry.get<rtype::components::Health>(enemy.first)) {
        health->hp -= damage ? damage->damage : 1;
        if (health->hp > 0)
            return;
    }
    frame.destroyed.push_back(enemy.first);
    frame.killedEnemies.emplace_back(enemy.second.left, enemy.second.top);
    _score += pointsFor(enemy.first);
}

std::uint32_t Game::pointsFor(engine::ecs::Entity enemy)
{
    const float loopBonus = (_gameMode == GameMode::Infinite)
                                ? completedLoops() * _rules.infinite.scoreMultiplierPerLoop
                                : 0.f;
    const float multiplier = currentDifficulty().scoreMultiplier + loopBonus;
    const auto *traits = _registry.get<rtype::components::EnemyTraits>(enemy);
    const std::uint32_t base = traits ? traits->score : 100;
    return static_cast<std::uint32_t>(std::round(static_cast<float>(base) * multiplier));
}

void Game::resolveShipContacts(CollisionFrame &frame)
{
    using rtype::components::Kind;
    using rtype::components::Position;

    _registry.each<Position, Kind>([&](engine::ecs::Entity ship, const Position &, const Kind &kind) {
        if (kind.type != protocol::EntityType::Player || isInvulnerable(ship))
            return;
        const Box box = hitArea(ship);
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

    _registry.each<Position, Kind>([&](engine::ecs::Entity missile, const Position &, const Kind &kind) {
        if (kind.type != protocol::EntityType::EnemyMissile)
            return;
        const Box box = hitArea(missile);
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

    _registry.each<Position, Kind>([&](engine::ecs::Entity ship, const Position &, const Kind &kind) {
        if (kind.type != protocol::EntityType::Player || isInvulnerable(ship))
            return;
        if (engine::physics::overlaps(box, hitArea(ship))) {
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
        _weapons[player] = _arsenal.defaultWeapon; // a destroyed ship loses its bonus weapon
        _lives[player] = std::max(0, _lives[player] - 1);
        _respawnTimers[player] = _rules.respawnDelay;
    }
}

} // namespace rtype::server
