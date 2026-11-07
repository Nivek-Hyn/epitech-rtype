#include "levelLoader.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace rtype::server {

LevelConfig loadLevelFromJson(const std::string &filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Erreur critique : Impossible d'ouvrir le fichier de niveau : " + filepath);
    }

    nlohmann::json j;
    try {
        file >> j;
    } catch (const std::exception &e) {
        throw std::runtime_error("Erreur : Le fichier JSON " + filepath + " est malformé ! Détails : " + e.what());
    }

    LevelConfig level;
    level.stage = j.value("stage", 1);
    level.level_name = j.value("level_name", "Niveau inconnu");

    if (!j.contains("waves") || !j["waves"].is_array()) {
        throw std::runtime_error("Erreur : Le schéma du niveau est invalide (clé 'waves' manquante ou incorrecte).");
    }

    for (const auto &w : j["waves"]) {
        WaveConfig wave;
        wave.wave_id = w.value("wave_id", 0);
        wave.delay_before_start = w.value("delay_before_start", 0.f);

        const auto readSpawns = [&w](const char *key, std::vector<EnemySpawnConfig> &spawns) {
            if (!w.contains(key) || !w[key].is_array())
                return;
            for (const auto &e : w[key]) {
                EnemySpawnConfig spawn;
                spawn.type = e.value("type", "standard");
                spawn.spawn_y = e.value("spawn_y", 300.f);
                spawn.speed = e.value("speed", 0.f); // 0: use the speed from entities.json
                spawn.amplitude = e.value("amplitude", 0.f);
                spawn.frequency = e.value("frequency", 0.f);
                spawn.health = e.value("hp", e.value("health", 0));
                spawns.push_back(spawn);
            }
        };
        readSpawns("enemies", wave.enemies);
        readSpawns("obstacles", wave.obstacles);
        level.waves.push_back(wave);
    }
    return level;
}

} // namespace rtype::server