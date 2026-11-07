// Waves: making enemies and tiles appear, and detecting the game over.

#include "Game.hpp"
#include "common/Components.hpp"
#include <iostream>
#include <random>

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
        spawnFromLevel(enemy);
    for (const auto &obstacle : wave.obstacles)
        spawnFromLevel(obstacle);
    wave.isSpawned = true;
}

// The prefab named by the level appears on the right edge, with what the level overrides.
void Game::spawnFromLevel(const EnemySpawnConfig &config)
{
    const float spawnY = config.spawn_y < 0.f ? randomSpawnY() : config.spawn_y;
    const auto entity = spawnPrefab(config.type, WorldWidth, spawnY);
    if (!entity) {
        std::cerr << "Warning: unknown prefab '" << config.type << "' in level '" << _currentLevel.level_name << "'\n";
        return;
    }
    applyLevelOverrides(*entity, config);
    applyDifficulty(*entity);
}

// A value of 0 in the level keeps the value of the prefab.
void Game::applyLevelOverrides(engine::ecs::Entity entity, const EnemySpawnConfig &config)
{
    auto *health = _registry.get<rtype::components::Health>(entity);
    if (health && config.health > 0)
        health->hp = config.health;
    auto *velocity = _registry.get<rtype::components::Velocity>(entity);
    if (velocity && config.speed > 0.f)
        velocity->x = -config.speed;
    if (auto *pattern = _registry.get<rtype::components::MovementPattern>(entity)) {
        if (config.amplitude > 0.f)
            pattern->amplitude = config.amplitude;
        if (config.frequency > 0.f)
            pattern->frequency = config.frequency;
    }
}

// Health, speeds and attacks scaled by the difficulty and the infinite mode loops.
void Game::applyDifficulty(engine::ecs::Entity entity)
{
    if (auto *health = _registry.get<rtype::components::Health>(entity))
        health->hp = scaledHealth(health->hp);
    auto *velocity = _registry.get<rtype::components::Velocity>(entity);
    if (velocity)
        velocity->x = -scaledSpeed(-velocity->x);
    if (auto *pattern = _registry.get<rtype::components::MovementPattern>(entity); pattern && velocity)
        pattern->baseSpeed = -velocity->x;
    if (auto *attack = _registry.get<rtype::components::EnemyAttack>(entity)) {
        for (auto &entry : attack->attacks) {
            entry.fireRate = scaledFireRate(entry.fireRate);
            entry.bulletSpeed = scaledBulletSpeed(entry.bulletSpeed);
        }
    }
}

} // namespace rtype::server
