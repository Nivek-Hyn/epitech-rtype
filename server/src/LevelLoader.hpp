#pragma once

#include <string>
#include <vector>

namespace rtype::server {

// One enemy or tile of a wave, as written in assets/levels/*.json. A value of 0 keeps the value of the prefab.
struct EnemySpawnConfig {
    std::string type;     // prefab name in assets/config/entities.json
    float spawnY = 300.f; // height on the right edge; negative: a random height
    float speed = 0.f;
    float amplitude = 0.f; // movement pattern parameters
    float frequency = 0.f;
    int health = 0;
};

struct WaveConfig {
    int id = 0;
    float delay = 0.f; // seconds from the start of the level
    std::vector<EnemySpawnConfig> enemies;
    std::vector<EnemySpawnConfig> obstacles; // destructible tiles: they do not hold the level back
    bool isSpawned = false;
};

struct LevelConfig {
    int stage = 1;
    std::string name;
    std::vector<WaveConfig> waves;
};

// Reads a level file. Throws std::runtime_error naming the file when it is missing or invalid.
LevelConfig loadLevelFromJson(const std::string &filepath);

} // namespace rtype::server
