#pragma once

#include "common/Constants.hpp"
#include "common/Protocol.hpp"
#include "engine/ecs/Registry.hpp"
#include "levelLoader.hpp"
#include "net/INetworkServer.hpp"

#include <array>
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>

namespace rtype::server {

// The authoritative game simulation. It knows nothing about sockets:
// main.cpp feeds it network events and sends what it produces, so it can be tested without a network.
class Game {
public:
    enum class GameMode {
        Campaign,
        Infinite
    };

    enum class Difficulty {
        Easy,
        Normal,
        Hard
    };

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
        std::vector<std::string> campaignLevels = {
            "assets/levels/level1.json",
            "assets/levels/level2.json",
            "assets/levels/level3.json"
        };
        float intermissionDelay = 2.0f;
        int playerLives = 3;
        float respawnDelay = 2.0f;
        float invulnerability = 2.0f; // seconds without damage after a respawn
    };

    struct AttackDefinition {
        float fireRate = 2.0f;
        float bulletSpeed = 350.f;
        std::uint8_t bulletVariant = 0;
        std::string bulletPattern = "straight";
        float offsetY = 0.f;
    };

    struct EnemyDefinition {
        std::uint8_t variant = 0;
        int health = 1;
        float speed = 100.f;
        std::string pattern = "straight";
        float amplitude = 0.f; // 0: default of the movement pattern
        float frequency = 0.f; // 0: default of the movement pattern
        std::uint32_t score = 100;
        bool isBoss = false;
        float holdX = 0.f; // boss only: where it stops and fights (0: keeps moving)
        bool hasAttack = false;
        std::vector<AttackDefinition> attacks;
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

    const GameRules &rules() const { return _rules; }
    GameRules &rules() { return _rules; }

private:
    engine::ecs::Entity spawnNetworked(protocol::EntityType type, std::uint8_t variant, float x, float y);
    void spawnEnemies(float dt);
    void handleCollisions();
    void updateLevelProgression(float dt);
    void clearEnemies();
    void updatePlayerLives(float dt);
    void resetWaves();
    bool isInvulnerable(engine::ecs::Entity entity) const;

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

    LevelConfig _currentLevel;
    float _levelTimer = 0.f;
    float _transitionTimer = 0.f;

    std::unordered_map<std::string, EnemyDefinition> _entityDefinitions;
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
