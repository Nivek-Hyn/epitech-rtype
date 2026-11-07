#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace rtype::server {

// A player weapon, read from assets/config/weapons.json. The missile itself is a prefab of entities.json:
// its sprite, hitbox, speed and damage are described there.
struct WeaponDefinition {
    std::string missile = "player_missile"; // prefab of each missile
    float cooldown = 0.25f;                 // seconds between two shots
    std::vector<float> angles{0.f};         // one missile per angle, in degrees (positive = downward)
};

struct Arsenal {
    std::string defaultWeapon = "blaster";
    std::unordered_map<std::string, WeaponDefinition> weapons{{"blaster", {}}};
    float dropChance = 0.f;               // probability that a destroyed enemy drops a bonus
    std::vector<std::string> dropPrefabs; // bonus prefabs, picked at random

    // The named weapon, or the default one when the name is unknown.
    const WeaponDefinition &weapon(const std::string &name) const;
};

// A missing or invalid file keeps the built-in blaster and drops no bonus.
Arsenal loadArsenal(const std::string &filepath);

} // namespace rtype::server
