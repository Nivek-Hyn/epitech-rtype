#include "Game.hpp"
#include "common/Components.hpp"
#include "engine/physics/Aabb.hpp"
#include "engine/systems/Movement.hpp"
#include "levelLoader.hpp"
#include "MovementSystem.hpp"

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

constexpr float EnemySpawnIntervalSeconds = 2.f;
constexpr float EnemySpeed = 100.f; // world units per second, towards the left
// Hitbox sizes match the client sprites (scaled x2): enemy 33x36, missile 18x12.
constexpr float EnemyWidth = 66.f;
constexpr float EnemyHeight = 72.f;
constexpr float MissileWidth = 36.f;
constexpr float MissileHeight = 24.f;
constexpr std::uint32_t PointsPerEnemy = 100;
// Rows where enemies appear, in turn. The first one is level with a new player's ship (y = 300).
constexpr std::array<float, 4> EnemyRows = {290.f, 150.f, 600.f, 450.f};

} // namespace

Game::Game()
{
    _players.fill(engine::ecs::NullEntity);
    _registry.addSystem(engine::systems::movement<rtype::components::Position, rtype::components::Velocity>);

    _currentLevel = loadLevelFromJson("assets/levels/level1.json");
    _levelTimer = 0.f;

    try {
        std::ifstream file("assets/config/entities.json");
        if (file.is_open()) {
            nlohmann::json j;
            file >> j;
            if (j.contains("enemies") && j["enemies"].is_object()) {
                for (auto &[key, val] : j["enemies"].items()) {
                    EnemyDefinition def;
                    def.health = val.value("health", 3);
                    def.speed = val.value("speed", 100.f);
                    def.pattern = val.value("pattern", "straight");
                    
                    _entityDefinitions[key] = def;
                }
            }
        }
    } catch (...) {
    }
}

void Game::loadLevel(const std::string &filepath)
{
    _currentLevel = loadLevelFromJson(filepath);
    _levelTimer = 0.f;

    for (auto &wave : _currentLevel.waves) {
        wave.isSpawned = false;
    }
}


// TODO(gameplay): spawn the player's ship (NetworkId, Kind, Position, Velocity...) and remember it in _players.
void Game::onPlayerJoined(PlayerId player)
{
    if (player >= MaxPlayers || _players[player] != engine::ecs::NullEntity)
        return;
    // we create a new entity in the ECS registry
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

// TODO(gameplay): turn the input flags into the ship's velocity, handle shooting.
void Game::onInput(PlayerId player, std::uint8_t flags)
{
    if (player >= MaxPlayers)
        return;
    // we get the entity associated with the player
    engine::ecs::Entity entity = _players[player];
    if (entity == engine::ecs::NullEntity)
        return;
    float speed = 200.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    // we check the input flags and set the velocity accordingly
    if (flags & protocol::InputFlag::Up)
        vy -= speed;
    if (flags & protocol::InputFlag::Down)
        vy += speed;
    if (flags & protocol::InputFlag::Left)
        vx -= speed;
    if (flags & protocol::InputFlag::Right)
        vx += speed;
    // on met a jour (ou ajout) la vitesse du joueur
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

    static float ennemyShootTimer = 0.f;
    ennemyShootTimer += dt;
    if (ennemyShootTimer >= 2.0f) {
        ennemyShootTimer = 0.f;
        _registry.each<rtype::components::Position, rtype::components::Kind>(
            [this](engine::ecs::Entity /*entity*/, const rtype::components::Position &pos, const rtype::components::Kind &kind) {
                if (kind.type == protocol::EntityType::Enemy) {
                    engine::ecs::Entity enemyMissile = spawnNetworked(protocol::EntityType::EnemyMissile, 0, pos.x - 30.0f, pos.y);
                    _registry.emplace<rtype::components::Velocity>(enemyMissile, -350.0f, 0.0f);
                }
            });
    }

    _registry.update(dt);

    constexpr float PlayerWidth = 33.f;
    constexpr float PlayerHeight = 36.f;

    _registry.each<rtype::components::Position, rtype::components::Kind>(
        [](engine::ecs::Entity /*entity*/, rtype::components::Position &pos, const rtype::components::Kind &kind) {
            if (kind.type == protocol::EntityType::Player) {
                pos.x = std::clamp(pos.x, 0.f, WorldWidth - PlayerWidth);
                pos.y = std::clamp(pos.y, 0.f, WorldHeight - PlayerHeight);
            }
        });

    handleCollisions();
    for (float &cooldown : _shootCooldowns)
        cooldown = std::max(0.f, cooldown - dt);

    std::vector<std::pair<engine::ecs::Entity, std::uint32_t>> expiredMissiles;
    _registry.each<rtype::components::Position, rtype::components::Kind, rtype::components::NetworkId>(
        [&expiredMissiles](engine::ecs::Entity entity, const rtype::components::Position &position,
                           const rtype::components::Kind &kind, const rtype::components::NetworkId &networkId) {
            const bool missileOut =
                (kind.type == protocol::EntityType::PlayerMissile || kind.type == protocol::EntityType::EnemyMissile) &&
                (position.x > WorldWidth || position.x < 0.f || position.y > WorldHeight || position.y < 0.f);
            const bool enemyOut = kind.type == protocol::EntityType::Enemy && position.x < -EnemyWidth;
            if (missileOut || enemyOut)
                expiredMissiles.emplace_back(entity, networkId.id);
        });
    for (const auto &[entity, networkId] : expiredMissiles) {
        _registry.kill(entity);
        _events.push_back(protocol::EntityDestroyed{networkId, protocol::DestroyReason::OutOfBounds});
    }
    _tick++;
}

// TODO(gameplay): one EntityState per entity having NetworkId + Kind + Position.
protocol::Snapshot Game::snapshot()
{
    protocol::Snapshot snap;
    snap.tick = _tick;
    snap.state = protocol::GameState::Playing;
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
    const bool anyPlayer = std::any_of(_players.begin(), _players.end(), [](engine::ecs::Entity player) { return player != engine::ecs::NullEntity; });
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
    
    for (auto &wave : _currentLevel.waves) {
        if (!wave.isSpawned && _levelTimer >= wave.delay_before_start) {
            for (const auto &enemyConfig : wave.enemies) {
                int hp = 3;
                float speed = enemyConfig.speed;
                std::string patternType = "straight";

                if (_entityDefinitions.find(enemyConfig.type) != _entityDefinitions.end()) {
                    hp = _entityDefinitions[enemyConfig.type].health;
                    speed = _entityDefinitions[enemyConfig.type].speed;
                    patternType = _entityDefinitions[enemyConfig.type].pattern;
                }

                float randomizedY = disVertical(gen);
                engine::ecs::Entity enemy =
                    spawnNetworked(protocol::EntityType::Enemy, 0, WorldWidth + 100.0f, randomizedY);
                
                _registry.emplace<rtype::components::Velocity>(enemy, -speed, 0.f);
                _registry.emplace<rtype::components::Health>(enemy, hp);
                _registry.emplace<rtype::components::MovementPattern>(enemy, rtype::components::MovementPattern{patternType});
            }
            wave.isSpawned = true;
        }
    }
}

// A player missile touching an enemy inflicts damage and adds PointsPerEnemy to the score if destroyed.
void Game::handleCollisions()
{
    using rtype::components::Kind;
    using rtype::components::Position;

    std::vector<std::pair<engine::ecs::Entity, engine::physics::Aabb>> enemies;
    _registry.each<Position, Kind>([&enemies](engine::ecs::Entity entity, const Position &position, const Kind &kind) {
        if (kind.type == protocol::EntityType::Enemy)
            enemies.push_back({entity, {position.x, position.y, EnemyWidth, EnemyHeight}});
    });

    std::vector<bool> enemyHit(enemies.size(), false);
    std::vector<engine::ecs::Entity> destroyed;

    _registry.each<Position, Kind>([&](engine::ecs::Entity missile, const Position &position, const Kind &kind) {
        if (kind.type != protocol::EntityType::PlayerMissile)
            return;
        const engine::physics::Aabb missileBox{position.x, position.y, MissileWidth, MissileHeight};
        for (std::size_t i = 0; i < enemies.size(); i++) {
            if (!enemyHit[i] && engine::physics::overlaps(missileBox, enemies[i].second)) {
                enemyHit[i] = true;
                destroyed.push_back(missile); // Le missile du joueur disparaît

                // Tâche #54 : Gestion des dégâts multiples sur l'ennemi
                if (auto *health = _registry.get<rtype::components::Health>(enemies[i].first)) {
                    health->hp -= 1;
                    if (health->hp <= 0) {
                        destroyed.push_back(enemies[i].first);
                        _score += PointsPerEnemy;
                    }
                } else {
                    destroyed.push_back(enemies[i].first);
                    _score += PointsPerEnemy;
                }
                return; // un missile détruit au plus un ennemi
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
                destroyed.push_back(playerEntity);     // Tue le joueur
                destroyed.push_back(enemies[i].first); // Tue l'ennemi
            }
        }
    });
    // Les missiles ennemis touchent le joueur
    _registry.each<Position, Kind>([&](engine::ecs::Entity missile, const Position &position, const Kind &kind) {
        if (kind.type != protocol::EntityType::EnemyMissile)
            return;
        
        const engine::physics::Aabb missileBox{position.x, position.y, MissileWidth, MissileHeight};

        _registry.each<Position, Kind>([&](engine::ecs::Entity playerEntity, const Position &playerPos, const Kind &playerKind) {
            if (playerKind.type != protocol::EntityType::Player)
                return;

            const engine::physics::Aabb playerBox{playerPos.x, playerPos.y, PlayerWidth, PlayerHeight};

            if (engine::physics::overlaps(missileBox, playerBox)) {
                destroyed.push_back(missile);       // Le missile ennemi disparaît
                destroyed.push_back(playerEntity);   // Le joueur est touché et détruit
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