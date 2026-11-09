#include "Weapons.hpp"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace rtype::server {

namespace {

using Json = nlohmann::json;

WeaponDefinition parseWeapon(const Json &json)
{
    WeaponDefinition weapon;
    weapon.missile = json.value("missile", weapon.missile);
    weapon.cooldown = json.value("cooldown", weapon.cooldown);
    weapon.angles = json.value("angles", weapon.angles);
    if (weapon.angles.empty())
        weapon.angles = {0.f};
    return weapon;
}

Arsenal parseArsenal(const Json &json)
{
    Arsenal arsenal;
    const Json weapons = json.value("weapons", Json::object());
    for (const auto &[name, weapon] : weapons.items())
        arsenal.weapons[name] = parseWeapon(weapon);
    arsenal.defaultWeapon = json.value("default_weapon", arsenal.defaultWeapon);
    if (!arsenal.weapons.contains(arsenal.defaultWeapon))
        arsenal.weapons[arsenal.defaultWeapon] = {};

    const Json drops = json.value("bonus_drops", Json::object());
    arsenal.dropChance = drops.value("chance", 0.f);
    arsenal.dropPrefabs = drops.value("prefabs", std::vector<std::string>{});
    return arsenal;
}

} // namespace

const WeaponDefinition &Arsenal::weapon(const std::string &name) const
{
    if (const auto found = weapons.find(name); found != weapons.end())
        return found->second;
    return weapons.at(defaultWeapon);
}

Arsenal loadArsenal(const std::string &filepath)
{
    std::ifstream file(filepath);
    if (!file) {
        std::cerr << "Warning: could not open " << filepath << ", using the default blaster\n";
        return {};
    }
    try {
        return parseArsenal(Json::parse(file));
    } catch (const Json::exception &error) {
        std::cerr << "Warning: invalid " << filepath << ": " << error.what() << '\n';
        return {};
    }
}

} // namespace rtype::server
