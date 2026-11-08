#include "editor/Level.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <stdexcept>

namespace editor {

namespace {

nlohmann::json readJson(const std::string &path)
{
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("Cannot open " + path);
    try {
        return nlohmann::json::parse(file);
    } catch (const nlohmann::json::exception &error) {
        throw std::runtime_error("Invalid JSON in " + path + ": " + error.what());
    }
}

void readWave(const nlohmann::json &wave, Level &level)
{
    const float time = wave.value("delay_before_start", 0.f);
    for (const char *list : {"enemies", "obstacles"}) {
        if (!wave.contains(list) || !wave[list].is_array())
            continue;
        for (const nlohmann::json &fields : wave[list])
            level.enemies.push_back({time, fields, list});
    }
}

// The wave list of a prefab, from the type of its Kind; empty for what a level does not place.
std::string listOf(const nlohmann::json &prefab)
{
    const std::string type = prefab.value("Kind", nlohmann::json::object()).value("type", "");
    if (type == "enemy")
        return "enemies";
    if (type == "obstacle")
        return "obstacles";
    return "";
}

} // namespace

Level loadLevel(const std::string &path)
{
    const nlohmann::json json = readJson(path);
    if (!json.contains("waves") || !json["waves"].is_array())
        throw std::runtime_error(path + " has no 'waves' array");

    Level level;
    level.stage = json.value("stage", 1);
    level.name = json.value("level_name", level.name);
    for (const nlohmann::json &wave : json["waves"])
        readWave(wave, level);
    return level;
}

nlohmann::json toJson(const Level &level)
{
    std::map<float, nlohmann::json> waves; // sorted by time
    for (const Placement &placement : level.enemies)
        waves[placement.time][placement.list].push_back(placement.fields);

    nlohmann::json result = {{"stage", level.stage}, {"level_name", level.name}, {"waves", nlohmann::json::array()}};
    int waveId = 1;
    for (auto &[time, lists] : waves) {
        nlohmann::json wave = {{"wave_id", waveId++}, {"delay_before_start", time}};
        wave.update(lists);
        result["waves"].push_back(wave);
    }
    return result;
}

void saveLevel(const Level &level, const std::string &path)
{
    std::ofstream file(path);
    if (!file)
        throw std::runtime_error("Cannot write " + path);
    file << toJson(level).dump(2) << '\n';
}

std::vector<EnemyType> loadEnemyTypes(const std::string &path)
{
    const nlohmann::json json = readJson(path);
    std::vector<EnemyType> types;
    const nlohmann::json prefabs = json.value("prefabs", nlohmann::json::object());
    for (const auto &[name, prefab] : prefabs.items()) {
        const std::string list = listOf(prefab);
        const float speed = -prefab.value("Velocity", nlohmann::json::object()).value("x", -100.f);
        if (!list.empty())
            types.push_back({name, speed, list});
    }
    if (types.empty())
        types.push_back({"standard", 100.f, "enemies"});
    return types;
}

} // namespace editor
