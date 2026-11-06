// Waves: making enemies and tiles appear, and detecting the game over.

#include "Game.hpp"
#include "common/Components.hpp"
#include <iostream>
#include <random>
#include <utility>
#include <vector>

namespace rtype::server {

namespace {

// Used when a level asks for a negative spawn_y.
float randomSpawnY()
{
    static std::random_device device;
    static std::mt19937 generator(device());
    static std::uniform_real_distribution<float> distribution(50.0f, 650.0f);
    return distribution(generator);
}

} // namespace

void Game::updateWaves(float dt)
{
    if (_state != protocol::GameState::Playing)
        return;

    if (!anyPlayerConnected()) {
        // The level restarts for the next player: nothing may stay from the previous attempt.
        clearEnemies();
        resetWaves();
        return;
    }
    if (!anyPlayerStillInGame()) {
        clearEnemies();
        _state = protocol::GameState::GameOver;
        return;
    }

    _levelTimer += dt;
    for (auto &wave : _currentLevel.waves) {
        if (!wave.isSpawned && _levelTimer >= wave.delay_before_start)
            spawnWave(wave);
    }
}

void Game::spawnWave(WaveConfig &wave)
{
    for (const auto &enemy : wave.enemies)
        spawnEnemy(enemy);
    for (const auto &obstacle : wave.obstacles)
        spawnObstacle(obstacle);
    wave.isSpawned = true;
}

// The enemy type from entities.json, with what the level overrides. 0 keeps the value of the type.
Game::EnemyDefinition Game::resolveEnemy(const EnemySpawnConfig &config)
{
    EnemyDefinition enemy;
    const auto known = _entityDefinitions.find(config.type);
    if (known != _entityDefinitions.end()) {
        enemy = known->second;
        if (config.health > 0)
            enemy.health = config.health;
    } else {
        std::cerr << "Warning: unknown enemy type '" << config.type << "' in level '" << _currentLevel.level_name << "'\n";
    }
    if (config.speed > 0.f)
        enemy.speed = config.speed;
    if (config.amplitude > 0.f)
        enemy.amplitude = config.amplitude;
    if (config.frequency > 0.f)
        enemy.frequency = config.frequency;
    return enemy;
}

void Game::spawnEnemy(const EnemySpawnConfig &config)
{
    const EnemyDefinition definition = resolveEnemy(config);
    const float speed = scaledSpeed(definition.speed);
    const float spawnY = config.spawn_y < 0.f ? randomSpawnY() : config.spawn_y;

    const engine::ecs::Entity enemy = spawnNetworked(protocol::EntityType::Enemy, definition.variant, WorldWidth, spawnY);
    _registry.emplace<rtype::components::Velocity>(enemy, -speed, 0.f);
    _registry.emplace<rtype::components::Health>(enemy, scaledHealth(definition.health));
    _registry.emplace<rtype::components::MovementPattern>(
        enemy, rtype::components::MovementPattern{definition.pattern, definition.amplitude, definition.frequency, speed,
                                                  definition.holdX, 0.f, 0.f, false, 0.f});
    _registry.emplace<rtype::components::EnemyTraits>(enemy, rtype::components::EnemyTraits{definition.score, definition.isBoss});
    if (definition.hasAttack && !definition.attacks.empty())
        giveAttacks(enemy, definition.attacks);
}

void Game::giveAttacks(engine::ecs::Entity enemy, const std::vector<AttackDefinition> &attacks)
{
    std::vector<rtype::components::AttackEntry> entries;
    for (const auto &attack : attacks) {
        entries.push_back(rtype::components::AttackEntry{0.f, scaledFireRate(attack.fireRate), scaledBulletSpeed(attack.bulletSpeed),
                                                         attack.bulletVariant, attack.bulletPattern, attack.offsetY});
    }
    _registry.emplace<rtype::components::EnemyAttack>(enemy, rtype::components::EnemyAttack{std::move(entries)});
}

void Game::spawnObstacle(const EnemySpawnConfig &config)
{
    const auto definition = _obstacleDefinitions.find(config.type);
    if (definition == _obstacleDefinitions.end()) {
        std::cerr << "Warning: unknown obstacle type '" << config.type << "' in level '" << _currentLevel.level_name << "'\n";
        return;
    }
    const int health = config.health > 0 ? config.health : definition->second.health;
    const float speed = config.speed > 0.f ? config.speed : definition->second.speed;
    const engine::ecs::Entity obstacle =
        spawnNetworked(protocol::EntityType::Obstacle, definition->second.variant, WorldWidth, config.spawn_y);
    _registry.emplace<rtype::components::Velocity>(obstacle, -scaledSpeed(speed), 0.f);
    _registry.emplace<rtype::components::Health>(obstacle, scaledHealth(health));
}

} // namespace rtype::server
