#pragma once

#include "Prefabs.hpp"
#include "Weapons.hpp"
#include "common/Constants.hpp"
#include "common/Protocol.hpp"
#include "engine/ecs/Registry.hpp"
#include "engine/physics/Aabb.hpp"
#include "levelLoader.hpp"
#include "net/INetworkServer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rtype::server {

// The authoritative game simulation. It knows nothing about sockets:
// main.cpp feeds it network events and sends what it produces, so it can be tested without a network.
class Game {
public:
    enum class GameMode { Campaign, Infinite };

    enum class Difficulty { Easy, Normal, Hard };

    struct DifficultyConfig {
        float healthMultiplier = 1.0f;
        float speedMultiplier = 1.0f;
        float fireRateMultiplier = 1.0f;
        float bulletSpeedMultiplier = 1.0f;
        float scoreMultiplier = 1.0f;
    };

    struct InfiniteConfig {
        float healthGrowthPerLoop = 0.2f;
        float speedGrowthPerLoop = 0.05f;
        float fireRateGrowthPerLoop = 0.1f;
        float minFireRate = 0.5f;
        float bulletSpeedGrowthPerLoop = 0.1f;
        float maxBulletSpeed = 700.0f;
        float scoreMultiplierPerLoop = 0.5f;
    };

    struct GameRules {
        std::unordered_map<Difficulty, DifficultyConfig> difficulties;
        InfiniteConfig infinite;
        std::vector<std::string> campaignLevels = {"assets/levels/level1.json", "assets/levels/level2.json",
                                                   "assets/levels/level3.json"};
        float intermissionDelay = 2.0f;
        int playerLives = 3;
        float respawnDelay = 2.0f;
        float invulnerability = 2.0f;
        std::size_t levelsPerWorld = 3;
    };

    Game();

    void onPlayerJoined(PlayerId player);
    void onPlayerLeft(PlayerId player);
    void onInput(PlayerId player, std::uint8_t flags);

    // Advances the simulation by one fixed step.
    void update(float dt);

    // Current state of every networked entity.
    protocol::Snapshot snapshot();

    // Events produced since the last call (EntityDestroyed...), to broadcast.
    std::vector<protocol::Message> takeEvents();

    std::uint32_t tick() const { return _tick; }

    void loadLevel(const std::string &filepath);
    void loadCampaign(const std::vector<std::string> &levelPaths);
    void loadRules(const std::string &filepath);
    // Adds the prefabs of an entities file (see assets/config/entities.json).
    void loadEntitiesConfig(const std::string &filepath);

    void setGameMode(GameMode mode) { _gameMode = mode; }
    GameMode gameMode() const { return _gameMode; }

    void setDifficulty(Difficulty diff) { _difficulty = diff; }
    Difficulty difficulty() const { return _difficulty; }

    std::uint32_t infiniteLoopCount() const { return _infiniteLoopCount; }
    void setInfiniteLoopCount(std::uint32_t count) { _infiniteLoopCount = count; }

    std::size_t currentLevelIndex() const { return _currentLevelIndex; }
    const LevelConfig &currentLevel() const { return _currentLevel; }
    protocol::GameState gameState() const { return _state; }
    int playerLives(PlayerId player) const { return player < MaxPlayers ? _lives[player] : 0; }
    const std::string &playerWeapon(PlayerId player) const { return _weapons.at(player); }
    // Weapons and bonus drops from assets/config/weapons.json.
    Arsenal &arsenal() { return _arsenal; }

    const GameRules &rules() const { return _rules; }
    GameRules &rules() { return _rules; }

private:
    struct CollisionFrame;

    // GameConfigLoader.cpp
    void applyDefaultRules();

    // GameLevels.cpp
    void loadCurrentLevel();
    void loadFirstLevel();
    void resetWaves();
    bool allWavesSpawned() const;
    bool anyEnemyAlive();
    void updateLevelProgression(float dt);
    void goToNextLevel();
    void restartCampaign();

    // GamePlayers.cpp
    void applyMovementInput(std::size_t player, std::uint8_t flags);
    void shootIfReady(std::size_t player);
    void spawnPlayerShip(std::size_t player);
    void removePlayerShip(std::size_t player);
    void updatePlayerLives(float dt);
    void updatePlayerRespawn(std::size_t player, float dt);
    bool isInvulnerable(engine::ecs::Entity entity) const;
    bool anyPlayerConnected() const;
    bool anyPlayerStillInGame() const;
    void restoreLivesOnNewWorld();

    // Game.cpp
    engine::ecs::Entity spawnNetworked(protocol::EntityType type, std::uint8_t variant, float x, float y);
    std::optional<engine::ecs::Entity> spawnPrefab(const std::string &name, float x, float y);
    engine::ecs::Entity spawnShot(protocol::EntityType type, std::uint8_t variant, float x, float y);
    engine::physics::Aabb hitArea(engine::ecs::Entity entity);
    void keepPlayersOnScreen();
    void updateShootCooldowns(float dt);
    void removeEntitiesOutOfScreen();
    void clearEnemies();
    void removeAsOutOfBounds(const std::vector<std::pair<engine::ecs::Entity, std::uint32_t>> &entities);

    // GameDifficulty.cpp
    DifficultyConfig &currentDifficulty();
    float completedLoops() const;
    int scaledHealth(int health);
    float scaledSpeed(float speed);
    float scaledFireRate(float fireRate);
    float scaledBulletSpeed(float bulletSpeed);

    // GameWaves.cpp
    void updateWaves(float dt);
    void spawnWave(WaveConfig &wave);
    void spawnFromLevel(const EnemySpawnConfig &config);
    void applyLevelOverrides(engine::ecs::Entity entity, const EnemySpawnConfig &config);
    void applyDifficulty(engine::ecs::Entity entity);

    // GameCollisions.cpp
    void handleCollisions();
    // GameWeapons.cpp
    void fire(std::size_t player, float x, float y);
    void dropBonuses(const std::vector<std::pair<float, float>> &positions);
    std::optional<std::size_t> playerTouching(const engine::physics::Aabb &box);
    void collectBonuses();
    void giveBonus(engine::ecs::Entity bonus, std::size_t player);

    CollisionFrame collectHitAreas();
    void resolvePlayerMissiles(CollisionFrame &frame);
    bool missileHitsObstacle(CollisionFrame &frame, engine::ecs::Entity missile, const engine::physics::Aabb &box);
    bool missileHitsEnemy(CollisionFrame &frame, engine::ecs::Entity missile, const engine::physics::Aabb &box);
    void damageEnemy(CollisionFrame &frame, const std::pair<engine::ecs::Entity, engine::physics::Aabb> &enemy,
                     engine::ecs::Entity missile);
    std::uint32_t pointsFor(engine::ecs::Entity enemy);
    void resolveShipContacts(CollisionFrame &frame);
    void shipRamsEnemies(CollisionFrame &frame, engine::ecs::Entity ship, const engine::physics::Aabb &box);
    void resolveEnemyMissiles(CollisionFrame &frame);
    void missileHitsShips(CollisionFrame &frame, engine::ecs::Entity missile, const engine::physics::Aabb &box);
    void destroyEntities(const std::vector<engine::ecs::Entity> &entities);
    void loseLifeIfShip(engine::ecs::Entity entity);

    engine::ecs::Registry _registry;
    std::array<engine::ecs::Entity, MaxPlayers> _players;
    std::array<float, MaxPlayers> _shootCooldowns{};
    std::array<bool, MaxPlayers> _connected{};
    std::array<int, MaxPlayers> _lives{};
    std::array<float, MaxPlayers> _respawnTimers{};
    std::array<float, MaxPlayers> _invulnerableTimers{};
    std::vector<protocol::Message> _events;
    std::uint32_t _tick = 0;
    std::uint32_t _nextNetId = 1;
    float _enemySpawnTimer = 0.f;
    std::uint32_t _enemiesSpawned = 0;
    std::uint32_t _score = 0;
    Arsenal _arsenal;
    std::array<std::string, MaxPlayers> _weapons;
    std::mt19937 _random{std::random_device{}()};

    LevelConfig _currentLevel;
    float _levelTimer = 0.f;
    float _transitionTimer = 0.f;

    Prefabs _prefabs;
    GameRules _rules;
    std::vector<std::string> _campaignLevels;
    std::size_t _currentLevelIndex = 0;

    GameMode _gameMode = GameMode::Campaign;
    Difficulty _difficulty = Difficulty::Normal;
    std::uint32_t _infiniteLoopCount = 1;
    protocol::GameState _state = protocol::GameState::Playing;
};

LevelConfig loadLevelFromJson(const std::string &filepath);

} // namespace rtype::server
