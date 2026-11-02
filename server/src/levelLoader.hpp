#pragma once
#include <string>
#include <vector>

namespace rtype::server {

struct EnemySpawnConfig {
    std::string type;
    float spawn_y;
    float speed;
    float amplitude;  //pour le sinusoidal
    float frequency;  //pour le sinusoidal
};

struct WaveConfig {
    int wave_id;
    float delay_before_start;
    std::vector<EnemySpawnConfig> enemies;
    bool isSpawned = false; // Pour savoir si la vague a déjà été lancée
};

struct LevelConfig {
    int stage;
    std::string level_name;
    std::vector<WaveConfig> waves;
};

LevelConfig loadLevelFromJson(const std::string& filepath);

}