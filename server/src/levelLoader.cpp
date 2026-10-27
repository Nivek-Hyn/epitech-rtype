#include <fstream>
#include "levelLoader.hpp"
#include <nlohmann/json.hpp> //bibliotheque pour lire le json

namespace rtype::server {

LevelConfig loadLevelFromJson(const std::string& filepath) {
    std::ifstream file(filepath);
    nlohmann::json j;
    file >> j;

    LevelConfig level;
    level.stage = j["stage"];
    level.level_name = j["level_name"];

    for (const auto& w : j["waves"]) {
        WaveConfig wave;
        wave.wave_id = w["wave_id"];
        wave.delay_before_start = w["delay_before_start"];

        for (const auto& e : w["enemies"]) {
            EnemySpawnConfig enemy;
            enemy.type = e["type"];
            enemy.spawn_y = e["spawn_y"];
            enemy.speed = e.value("speed", 150.f); // Valeur par défaut si absent
            enemy.amplitude = e.value("amplitude", 0.f);
            enemy.frequency = e.value("frequency", 0.f);
            wave.enemies.push_back(enemy);
        }
        level.waves.push_back(wave);
    }
    return level;
}
} // namespace rtype::server