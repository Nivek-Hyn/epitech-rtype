#pragma once
#include <string>
#include <vector>

namespace rtype::server {

struct EnemySpawnConfig {
    std::string type;
    float spawn_y;
    float speed;
    float amplitude; // pour le sinusoidal
    float frequency; // pour le sinusoidal
    int health = 0;  // 0 = utilise la valeur par defaut d'entities.json
};

struct WaveConfig {
    int wave_id;
    float delay_before_start;
    std::vector<EnemySpawnConfig> enemies;
    std::vector<EnemySpawnConfig> obstacles; // destructible tiles: they do not hold the level back
    bool isSpawned = false; // Pour savoir si la vague a déjà été lancée
};

struct LevelConfig {
    int stage;
    std::string level_name;
    std::vector<WaveConfig> waves;
};

LevelConfig loadLevelFromJson(const std::string &filepath);

} // namespace rtype::server