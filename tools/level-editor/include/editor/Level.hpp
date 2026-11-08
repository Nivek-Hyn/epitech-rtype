#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace editor {

// One enemy placed on the timeline. fields keeps the whole JSON object (type, spawn_y, speed, and any
// other key such as amplitude or hp), so saving never loses a value the editor does not display.
struct Placement {
    float time = 0.f; // seconds from the start of the level: the delay_before_start of its wave
    nlohmann::json fields;
    std::string list = "enemies"; // wave list it belongs to: "enemies", or "obstacles" for tiles

    std::string type() const { return fields.value("type", "standard"); }
    float y() const { return fields.value("spawn_y", 0.f); }
};

struct Level {
    int stage = 1;
    std::string name = "New level";
    std::vector<Placement> enemies;
};

// Reads the format of assets/levels/*.json. Throws std::runtime_error when the file cannot be read.
Level loadLevel(const std::string &path);
// Groups the placements that share the same time into one wave, sorted by time, numbered from 1.
nlohmann::json toJson(const Level &level);
void saveLevel(const Level &level, const std::string &path);

// The prefabs of assets/config/entities.json a level can place: enemies and tiles, with their speed.
struct EnemyType {
    std::string name;
    float speed = 100.f;
    std::string list = "enemies"; // "enemies" or "obstacles"
    int variant = 0;               // sprite, from the Kind of the prefab
    float width = 64.f;            // hit area, from the Hitbox of the prefab
    float height = 64.f;
    float offsetY = 0.f;
};
std::vector<EnemyType> loadEnemyTypes(const std::string &path);

} // namespace editor
