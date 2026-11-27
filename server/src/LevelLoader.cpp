#include "LevelLoader.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace rtype::server {

namespace {

using Json = nlohmann::json;

EnemySpawnConfig readSpawn(const Json &json)
{
    EnemySpawnConfig spawn;
    spawn.type = json.value("type", "standard");
    spawn.spawnY = json.value("spawn_y", spawn.spawnY);
    spawn.speed = json.value("speed", 0.f);
    spawn.amplitude = json.value("amplitude", 0.f);
    spawn.frequency = json.value("frequency", 0.f);
    spawn.health = json.value("hp", json.value("health", 0));
    return spawn;
}

std::vector<EnemySpawnConfig> readSpawns(const Json &wave, const char *key)
{
    std::vector<EnemySpawnConfig> spawns;
    if (wave.contains(key) && wave[key].is_array()) {
        for (const Json &spawn : wave[key])
            spawns.push_back(readSpawn(spawn));
    }
    return spawns;
}

LevelConfig readLevel(const Json &json)
{
    if (!json.contains("waves") || !json["waves"].is_array())
        throw std::runtime_error("no 'waves' array");
    LevelConfig level;
    level.stage = json.value("stage", 1);
    level.name = json.value("level_name", "Unnamed level");
    for (const Json &wave : json["waves"])
        level.waves.push_back({wave.value("wave_id", 0), wave.value("delay_before_start", 0.f),
                               readSpawns(wave, "enemies"), readSpawns(wave, "obstacles")});
    return level;
}

} // namespace

LevelConfig loadLevelFromJson(const std::string &filepath)
{
    std::ifstream file(filepath);
    if (!file)
        throw std::runtime_error("cannot open level " + filepath);
    try {
        return readLevel(Json::parse(file));
    } catch (const std::exception &error) {
        throw std::runtime_error("invalid level " + filepath + ": " + error.what());
    }
}

} // namespace rtype::server
