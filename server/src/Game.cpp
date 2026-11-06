#include "Game.hpp"
#include "common/Components.hpp"
#include "engine/physics/Aabb.hpp"
#include "engine/systems/Movement.hpp"
#include "levelLoader.hpp"
#include "MovementSystem.hpp"
#include "EnemyShootSystem.hpp"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>
#include <cmath>
#include <random>
#include <fstream>
#include <nlohmann/json.hpp>

namespace rtype::server {

namespace {

constexpr float EnemyWidth = 66.f;
constexpr float EnemyHeight = 72.f;
constexpr float MissileWidth = 36.f;
constexpr float MissileHeight = 24.f;
constexpr std::uint32_t PointsPerEnemy = 100;

} // namespace

Game::Game()
{
    _players.fill(engine::ecs::NullEntity);
    _registry.addSystem(engine::systems::movement<rtype::components::Position, rtype::components::Velocity>);

    // Default configuration calibrated for Game Designers
    _rules.difficulties[Difficulty::Easy] = {0.8f, 0.85f, 0.8f, 0.8f, 0.8f};
    _rules.difficulties[Difficulty::Normal] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    _rules.difficulties[Difficulty::Hard] = {1.25f, 1.15f, 1.25f, 1.2f, 1.5f};
    _rules.infinite = {0.2f, 0.05f, 0.1f, 0.5f, 0.1f, 700.0f, 0.5f};
    _rules.campaignLevels = {
        "assets/levels/level1.json",
        "assets/levels/level2.json",
        "assets/levels/level3.json"
    };
    _rules.intermissionDelay = 2.0f;

    loadEntitiesConfig("assets/config/entities.json");
    loadRules("assets/config/game_rules.json");

    _campaignLevels = _rules.campaignLevels;
    _currentLevelIndex = 0;
    if (!_campaignLevels.empty()) {
        try {
            _currentLevel = loadLevelFromJson(_campaignLevels[0]);
        } catch (...) {
            // Level file might not be found in some test environments
        }
    }
}

void Game::loadEntitiesConfig(const std::string &filepath)
{
    try {
        std::ifstream file(filepath);
        if (file.is_open()) {
            nlohmann::json j;
            file >> j;
            if (j.contains("enemies") && j["enemies"].is_object()) {
                for (auto &[key, val] : j["enemies"].items()) {
                    EnemyDefinition def;
                    def.health = val.value("health", 1);
                    def.speed = val.value("speed", 100.f);
                    def.pattern = val.value("pattern", "straight");
                    def.variant = val.value<std::uint8_t>("variant", 0);
                    
                    if (val.contains("attacks") && val["attacks"].is_array()) {
                        def.hasAttack = true;
                        for (const auto &atkVal : val["attacks"]) {
                            AttackDefinition atk;
                            atk.fireRate = atkVal.value("fire_rate", 2.0f);
                            atk.bulletSpeed = atkVal.value("bullet_speed", 350.f);
                            atk.bulletVariant = atkVal.value<std::uint8_t>("bullet_variant", 0);
                            atk.bulletPattern = atkVal.value("bullet_pattern", "straight");
                            atk.offsetY = atkVal.value("offset_y", 0.f);
                            def.attacks.push_back(atk);
                        }
                    } else if (val.contains("attack") && val["attack"].is_object()) {
                        def.hasAttack = true;
                        AttackDefinition atk;
                        atk.fireRate = val["attack"].value("fire_rate", 2.0f);
                        atk.bulletSpeed = val["attack"].value("bullet_speed", 350.f);
                        atk.bulletVariant = val["attack"].value<std::uint8_t>("bullet_variant", 0);
                        atk.bulletPattern = val["attack"].value("bullet_pattern", "straight");
                        atk.offsetY = val["attack"].value("offset_y", 0.f);
                        def.attacks.push_back(atk);
                    } else {
                        def.hasAttack = false;
                    }
                    _entityDefinitions[key] = def;
                }
            }
        }
    } catch (...) {
    }
}

void Game::loadRules(const std::string &filepath)
{
    try {
        std::ifstream file(filepath);
        if (!file.is_open())
            return;
        nlohmann::json j;
        file >> j;

        if (j.contains("difficulties") && j["difficulties"].is_object()) {
            auto parseDiff = [&](const std::string &name, Difficulty diff) {
                if (j["difficulties"].contains(name)) {
                    const auto &d = j["difficulties"][name];
                    DifficultyConfig cfg;
                    cfg.healthMultiplier = d.value("health_multiplier", 1.0f);
                    cfg.speedMultiplier = d.value("speed_multiplier", 1.0f);
                    cfg.fireRateMultiplier = d.value("fire_rate_multiplier", 1.0f);
                    cfg.bulletSpeedMultiplier = d.value("bullet_speed_multiplier", 1.0f);
                    cfg.scoreMultiplier = d.value("score_multiplier", 1.0f);
                    _rules.difficulties[diff] = cfg;
                }
            };
            parseDiff("easy", Difficulty::Easy);
            parseDiff("normal", Difficulty::Normal);
            parseDiff("hard", Difficulty::Hard);
        }

        if (j.contains("infinite_mode") && j["infinite_mode"].is_object()) {
            const auto &inf = j["infinite_mode"];
            _rules.infinite.healthGrowthPerLoop = inf.value("health_growth_per_loop", 0.2f);
            _rules.infinite.speedGrowthPerLoop = inf.value("speed_growth_per_loop", 0.05f);
            _rules.infinite.fireRateGrowthPerLoop = inf.value("fire_rate_growth_per_loop", 0.1f);
            _rules.infinite.minFireRate = inf.value("min_fire_rate", 0.5f);
            _rules.infinite.bulletSpeedGrowthPerLoop = inf.value("bullet_speed_growth_per_loop", 0.1f);
            _rules.infinite.maxBulletSpeed = inf.value("max_bullet_speed", 700.0f);
            _rules.infinite.scoreMultiplierPerLoop = inf.value("score_multiplier_per_loop", 0.5f);
        }

        if (j.contains("campaign") && j["campaign"].is_object()) {
            const auto &camp = j["campaign"];
            if (camp.contains("levels") && camp["levels"].is_array()) {
                _rules.campaignLevels.clear();
                for (const auto &lvl : camp["levels"]) {
                    _rules.campaignLevels.push_back(lvl.get<std::string>());
                }
            }
            _rules.intermissionDelay = camp.value("intermission_delay", 2.0f);
        }
    } catch (...) {
    }
}

void Game::loadLevel(const std::string &filepath)
{
    _campaignLevels = {filepath};
    _currentLevelIndex = 0;
    _currentLevel = loadLevelFromJson(filepath);
    _levelTimer = 0.f;
    _transitionTimer = 0.f;

    for (auto &wave : _currentLevel.waves) {
        wave.isSpawned = false;
    }
}

void Game::loadCampaign(const std::vector<std::string> &levelPaths)
{
    _campaignLevels = levelPaths;
    _currentLevelIndex = 0;
    if (!_campaignLevels.empty()) {
        _currentLevel = loadLevelFromJson(_campaignLevels[0]);
        _levelTimer = 0.f;
        _transitionTimer = 0.f;
        for (auto &wave : _currentLevel.waves) {
            wave.isSpawned = false;
        }
    }
}

void Game::onPlayerJoined(PlayerId player)
{
    if (player >= MaxPlayers || _players[player] != engine::ecs::NullEntity)
        return;
    engine::ecs::Entity entity =
        spawnNetworked(protocol::EntityType::Player, static_cast<std::uint8_t>(player), 100.0f, 300.0f);
    _players[player] = entity;
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

void Game::onPlayerLeft(PlayerId player)
{
    if (player >= MaxPlayers || _players[player] == engine::ecs::NullEntity)
        return;
    const engine::ecs::Entity entity = _players[player];
    const auto *networkId = _registry.get<rtype::components::NetworkId>(entity);
    if (networkId)
        _events.push_back(protocol::EntityDestroyed{networkId->id, protocol::DestroyReason::Killed});
    _registry.kill(entity);
    _players[player] = engine::ecs::NullEntity;
    _shootCooldowns[player] = 0.f;
}

void Game::onInput(PlayerId player, std::uint8_t flags)
{
    if (player >= MaxPlayers)
        return;
    engine::ecs::Entity entity = _players[player];
    if (entity == engine::ecs::NullEntity)
        return;
    float speed = 200.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    if (flags & protocol::InputFlag::Up)
        vy -= speed;
    if (flags & protocol::InputFlag::Down)
        vy += speed;
    if (flags & protocol::InputFlag::Left)
        vx -= speed;
    if (flags & protocol::InputFlag::Right)
        vx += speed;
    _registry.emplace<rtype::components::Velocity>(entity, vx, vy);

    if ((flags & protocol::InputFlag::Shoot) && _shootCooldowns[player] <= 0.f) {
        auto *pos = _registry.get<rtype::components::Position>(entity);
        if (pos) {
            engine::ecs::Entity bullet = spawnNetworked(protocol::EntityType::PlayerMissile, 0, pos->x + 30.0f, pos->y);
            _registry.emplace<rtype::components::Velocity>(bullet, 400.0f, 0.0f);
            _shootCooldowns[player] = 0.25f;
        }
    }
}

void Game::update(float dt)
{
    spawnEnemies(dt);
    updateEnemyMovements(_registry, dt);

    updateEnemyShooting(_registry, [this](protocol::EntityType type, std::uint8_t variant, float x, float y) {
        return spawnNetworked(type, variant, x, y);
    }, dt);

    _registry.update(dt);

    constexpr float PlayerWidth = 33.f;
    constexpr float PlayerHeight = 36.f;

    _registry.each<rtype::components::Position, rtype::components::Kind>([](engine::ecs::Entity /*entity*/, rtype::components::Position &pos, const rtype::components::Kind &kind) {
            if (kind.type == protocol::EntityType::Player) {
                pos.x = std::clamp(pos.x, 0.f, WorldWidth - PlayerWidth);
                pos.y = std::clamp(pos.y, 0.f, WorldHeight - PlayerHeight);
            }
        });

    handleCollisions();
    for (float &cooldown : _shootCooldowns)
        cooldown = std::max(0.f, cooldown - dt);

    updateLevelProgression(dt);

    std::vector<std::pair<engine::ecs::Entity, std::uint32_t>> expiredMissiles;
    _registry.each<rtype::components::Position, rtype::components::Kind, rtype::components::NetworkId>(
        [&expiredMissiles](engine::ecs::Entity entity, const rtype::components::Position &position,
                           const rtype::components::Kind &kind, const rtype::components::NetworkId &networkId) {
            const bool missileOut =
                (kind.type == protocol::EntityType::PlayerMissile || kind.type == protocol::EntityType::EnemyMissile) &&
                (position.x > WorldWidth || position.x < 0.f || position.y > WorldHeight || position.y < 0.f);
            const bool enemyOut = kind.type == protocol::EntityType::Enemy &&
                (position.x < -EnemyWidth || position.y < -300.f || position.y > WorldHeight + 300.f);
            if (missileOut || enemyOut)
                expiredMissiles.emplace_back(entity, networkId.id);
        });
    for (const auto &[entity, networkId] : expiredMissiles) {
        _registry.kill(entity);
        _events.push_back(protocol::EntityDestroyed{networkId, protocol::DestroyReason::OutOfBounds});
    }
    _tick++;
}

void Game::updateLevelProgression(float dt)
{
    if (_state == protocol::GameState::Finished)
        return;

    bool allWavesSpawned = !_currentLevel.waves.empty() && std::all_of(
        _currentLevel.waves.begin(), _currentLevel.waves.end(), [](const WaveConfig &w) {
            return w.isSpawned;
        });
    bool noMoreEnemies = true;
    _registry.each<rtype::components::Kind>([&noMoreEnemies](engine::ecs::Entity /*entity*/, const rtype::components::Kind &kind) {
        if (kind.type == protocol::EntityType::Enemy) {
            noMoreEnemies = false;
        }
    });

    if (allWavesSpawned && noMoreEnemies) {
        _transitionTimer += dt;
        if (_transitionTimer >= _rules.intermissionDelay) {
            _transitionTimer = 0.f;

            if (_gameMode == GameMode::Campaign) {
                if (_currentLevelIndex + 1 < _campaignLevels.size()) {
                    _currentLevelIndex++;
                    _currentLevel = loadLevelFromJson(_campaignLevels[_currentLevelIndex]);
                    _levelTimer = 0.f;
                    for (auto &wave : _currentLevel.waves)
                        wave.isSpawned = false;
                } else {
                    _state = protocol::GameState::Finished;
                }
            } else if (_gameMode == GameMode::Infinite) {
                if (_currentLevelIndex + 1 < _campaignLevels.size()) {
                    _currentLevelIndex++;
                    _currentLevel = loadLevelFromJson(_campaignLevels[_currentLevelIndex]);
                    _levelTimer = 0.f;
                    for (auto &wave : _currentLevel.waves)
                        wave.isSpawned = false;
                } else {
                    _infiniteLoopCount++;
                    _currentLevelIndex = 0;
                    if (!_campaignLevels.empty()) {
                        _currentLevel = loadLevelFromJson(_campaignLevels[0]);
                    }
                    _levelTimer = 0.f;
                    for (auto &wave : _currentLevel.waves)
                        wave.isSpawned = false;
                }
            }
        }
    } else {
        _transitionTimer = 0.f;
    }
}

protocol::Snapshot Game::snapshot()
{
    protocol::Snapshot snap;
    snap.tick = _tick;
    snap.state = _state;
    snap.score = _score;

    _registry.each<rtype::components::Position, rtype::components::Kind, rtype::components::NetworkId>(
        [&snap](engine::ecs::Entity /*entity*/, const rtype::components::Position &pos,
                const rtype::components::Kind &kind, const rtype::components::NetworkId &netId) {
            protocol::EntityState state;
            state.netId = netId.id;
            state.type = kind.type;
            state.variant = kind.variant;
            state.x = pos.x;
            state.y = pos.y;
            snap.entities.push_back(state);
        });
    return snap;
}

std::vector<protocol::Message> Game::takeEvents()
{
    std::vector<protocol::Message> events;
    events.swap(_events);
    return events;
}

void Game::spawnEnemies(float dt)
{
    const bool anyPlayer = std::any_of(_players.begin(), _players.end(), [](engine::ecs::Entity player) {
        return player != engine::ecs::NullEntity;
    });
    if (!anyPlayer) {
        _levelTimer = 0.f;
        for (auto &wave : _currentLevel.waves)
            wave.isSpawned = false;
        return;
    }

    _levelTimer += dt;

    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> disVertical(50.0f, 650.0f);

    const auto &diffCfg = _rules.difficulties[_difficulty];
    const float loopOffset = (_gameMode == GameMode::Infinite && _infiniteLoopCount > 1)
                                 ? static_cast<float>(_infiniteLoopCount - 1)
                                 : 0.0f;

    const float hpGrowth = 1.0f + loopOffset * _rules.infinite.healthGrowthPerLoop;
    const float speedGrowth = 1.0f + loopOffset * _rules.infinite.speedGrowthPerLoop;
    const float fireRateGrowth = 1.0f + loopOffset * _rules.infinite.fireRateGrowthPerLoop;
    const float bulletSpeedGrowth = 1.0f + loopOffset * _rules.infinite.bulletSpeedGrowthPerLoop;

    for (auto &wave : _currentLevel.waves) {
        if (!wave.isSpawned && _levelTimer >= wave.delay_before_start) {
            for (const auto &enemyConfig : wave.enemies) {
                int hp = 1;
                float speed = enemyConfig.speed > 0.f ? enemyConfig.speed : 100.f;
                std::string patternType = "straight";
                float amplitude = enemyConfig.amplitude;
                float frequency = enemyConfig.frequency;

                bool hasAttack = false;
                std::vector<AttackDefinition> attacks;
                std::uint8_t enemyVariant = 0;

                if (_entityDefinitions.find(enemyConfig.type) != _entityDefinitions.end()) {
                    const auto &def = _entityDefinitions[enemyConfig.type];
                    hp = def.health;
                    if (enemyConfig.health > 0) {
                        hp = enemyConfig.health;
                    }
                    if (enemyConfig.speed <= 0.f) {
                        speed = def.speed;
                    }
                    patternType = def.pattern;
                    hasAttack = def.hasAttack;
                    attacks = def.attacks;
                    enemyVariant = def.variant;
                }

                const int finalHp = std::max(1, static_cast<int>(std::round(static_cast<float>(hp) * diffCfg.healthMultiplier * hpGrowth)));
                const float finalSpeed = speed * diffCfg.speedMultiplier * speedGrowth;

                float spawnY = enemyConfig.spawn_y;
                if (spawnY < 0.f) {
                    spawnY = disVertical(gen);
                }

                engine::ecs::Entity enemy =
                    spawnNetworked(protocol::EntityType::Enemy, enemyVariant, WorldWidth, spawnY);
                
                _registry.emplace<rtype::components::Velocity>(enemy, -finalSpeed, 0.f);
                _registry.emplace<rtype::components::Health>(enemy, finalHp);
                _registry.emplace<rtype::components::MovementPattern>(
                    enemy, rtype::components::MovementPattern{patternType, amplitude, frequency});

                if (hasAttack && !attacks.empty()) {
                    std::vector<rtype::components::AttackEntry> attackEntries;
                    for (const auto &atk : attacks) {
                        const float finalFireRate = std::max(_rules.infinite.minFireRate, atk.fireRate / (diffCfg.fireRateMultiplier * fireRateGrowth));
                        const float finalBulletSpeed = std::min(_rules.infinite.maxBulletSpeed, atk.bulletSpeed * diffCfg.bulletSpeedMultiplier * bulletSpeedGrowth);
                        attackEntries.push_back(rtype::components::AttackEntry{0.f, finalFireRate, finalBulletSpeed, atk.bulletVariant, atk.bulletPattern, atk.offsetY});
                    }
                    _registry.emplace<rtype::components::EnemyAttack>(
                        enemy, rtype::components::EnemyAttack{std::move(attackEntries)});
                }
            }
            wave.isSpawned = true;
        }
    }
}

void Game::handleCollisions()
{
    using rtype::components::Kind;
    using rtype::components::Position;

    std::vector<std::pair<engine::ecs::Entity, engine::physics::Aabb>> enemies;
    _registry.each<Position, Kind>([&enemies](engine::ecs::Entity entity, const Position &position, const Kind &kind) {
        if (kind.type != protocol::EntityType::Enemy)
            return;
        float w = EnemyWidth;
        float h = EnemyHeight;
        float offsetY = 0.f;
        if (kind.variant == 0) {
            w = 66.f;
            h = 72.f;
        } else if (kind.variant == 1) {
            w = 64.f;
            h = 56.f;
        } else if (kind.variant == 2) {
            w = 64.f;
            h = 56.f;
        } else if (kind.variant == 3) {
            w = 56.f;
            h = 46.f;
        } else if (kind.variant == 4) {
            w = 72.f;
            h = 83.f;
        } else if (kind.variant == 5) {
            w = 200.f;
            h = 200.f;
            offsetY = -15.f;
        } else if (kind.variant == 6) {
            w = 280.f;
            h = 222.f;
            offsetY = -60.f;
        }
        enemies.push_back({entity, {position.x, position.y + offsetY, w, h}});
    });

    std::vector<bool> enemyHit(enemies.size(), false);
    std::vector<engine::ecs::Entity> destroyed;

    const auto &diffCfg = _rules.difficulties[_difficulty];
    float scoreMult = diffCfg.scoreMultiplier;
    if (_gameMode == GameMode::Infinite && _infiniteLoopCount > 1) {
        scoreMult += static_cast<float>(_infiniteLoopCount - 1) * _rules.infinite.scoreMultiplierPerLoop;
    }
    const auto awardedPoints = static_cast<std::uint32_t>(std::round(static_cast<float>(PointsPerEnemy) * scoreMult));

    _registry.each<Position, Kind>([&](engine::ecs::Entity missile, const Position &position, const Kind &kind) {
        if (kind.type != protocol::EntityType::PlayerMissile)
            return;
        const engine::physics::Aabb missileBox{position.x, position.y, MissileWidth, MissileHeight};
        for (std::size_t i = 0; i < enemies.size(); i++) {
            if (!enemyHit[i] && engine::physics::overlaps(missileBox, enemies[i].second)) {
                enemyHit[i] = true;
                destroyed.push_back(missile);

                if (auto *health = _registry.get<rtype::components::Health>(enemies[i].first)) {
                    health->hp -= 1;
                    if (health->hp <= 0) {
                        destroyed.push_back(enemies[i].first);
                        _score += awardedPoints;
                    }
                } else {
                    destroyed.push_back(enemies[i].first);
                    _score += awardedPoints;
                }
                return;
            }
        }
    });

    constexpr float PlayerWidth = 33.f;
    constexpr float PlayerHeight = 36.f;

    _registry.each<Position, Kind>([&](engine::ecs::Entity playerEntity, const Position &position, const Kind &kind) {
        if (kind.type != protocol::EntityType::Player)
            return;

        const engine::physics::Aabb playerBox{position.x, position.y, PlayerWidth, PlayerHeight};

        for (std::size_t i = 0; i < enemies.size(); i++) {
            if (!enemyHit[i] && engine::physics::overlaps(playerBox, enemies[i].second)) {
                enemyHit[i] = true;
                destroyed.push_back(playerEntity);
                destroyed.push_back(enemies[i].first);
            }
        }
    });

    _registry.each<Position, Kind>([&](engine::ecs::Entity missile, const Position &position, const Kind &kind) {
        if (kind.type != protocol::EntityType::EnemyMissile)
            return;
        
        const engine::physics::Aabb missileBox{position.x, position.y, MissileWidth, MissileHeight};

        _registry.each<Position, Kind>([&](engine::ecs::Entity playerEntity, const Position &playerPos, const Kind &playerKind) {
            if (playerKind.type != protocol::EntityType::Player)
                return;

            const engine::physics::Aabb playerBox{playerPos.x, playerPos.y, PlayerWidth, PlayerHeight};

            if (engine::physics::overlaps(missileBox, playerBox)) {
                destroyed.push_back(missile);
                destroyed.push_back(playerEntity);
            }
        });
    });

    for (const engine::ecs::Entity entity : destroyed) {
        for (std::size_t player = 0; player < _players.size(); player++) {
            if (_players[player] == entity) {
                _players[player] = engine::ecs::NullEntity;
                _shootCooldowns[player] = 0.f;
            }
        }
        if (const auto *networkId = _registry.get<rtype::components::NetworkId>(entity))
            _events.push_back(protocol::EntityDestroyed{networkId->id, protocol::DestroyReason::Killed});
        _registry.kill(entity);
    }
}

} // namespace rtype::server
