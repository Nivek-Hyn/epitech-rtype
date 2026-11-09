#include "Prefabs.hpp"

#include "GameComponents.hpp"

#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace rtype::server {

namespace {

using Json = nlohmann::json;
namespace components = rtype::components;

protocol::EntityType entityType(const std::string &name)
{
    static const std::array<std::pair<const char *, protocol::EntityType>, 6> types = {{
        {"player", protocol::EntityType::Player},
        {"enemy", protocol::EntityType::Enemy},
        {"player_missile", protocol::EntityType::PlayerMissile},
        {"enemy_missile", protocol::EntityType::EnemyMissile},
        {"obstacle", protocol::EntityType::Obstacle},
        {"bonus", protocol::EntityType::Bonus},
    }};
    for (const auto &[typeName, type] : types) {
        if (name == typeName)
            return type;
    }
    throw std::invalid_argument("unknown entity type '" + name + "'");
}

components::Kind readKind(const Json &json)
{
    return {entityType(json.at("type").get<std::string>()), json.value<std::uint8_t>("variant", 0)};
}

components::Hitbox readHitbox(const Json &json)
{
    return {json.value("width", 0.f), json.value("height", 0.f), json.value("offset_y", 0.f)};
}

components::MovementPattern readMovementPattern(const Json &json)
{
    components::MovementPattern pattern;
    pattern.type = json.value("type", pattern.type);
    pattern.amplitude = json.value("amplitude", 0.f);
    pattern.frequency = json.value("frequency", 0.f);
    pattern.holdX = json.value("hold_x", 0.f);
    return pattern;
}

components::AttackEntry readAttack(const Json &json)
{
    components::AttackEntry attack;
    attack.fireRate = json.value("fire_rate", attack.fireRate);
    attack.bulletSpeed = json.value("bullet_speed", attack.bulletSpeed);
    attack.bulletVariant = json.value<std::uint8_t>("bullet_variant", 0);
    attack.bulletPattern = json.value("bullet_pattern", attack.bulletPattern);
    attack.offsetY = json.value("offset_y", 0.f);
    return attack;
}

// A list of attacks, or a single attack object.
components::EnemyAttack readEnemyAttack(const Json &json)
{
    std::vector<components::AttackEntry> attacks;
    if (json.is_array()) {
        for (const Json &attack : json)
            attacks.push_back(readAttack(attack));
    } else {
        attacks.push_back(readAttack(json));
    }
    return components::EnemyAttack{std::move(attacks)};
}

Prefabs::Components toComponents(const Json &prefab)
{
    Prefabs::Components components;
    for (const auto &[name, data] : prefab.items())
        components.emplace_back(name, data);
    return components;
}

// One broken prefab is skipped with a warning; the others stay usable.
void addPrefab(Prefabs &prefabs, const std::string &name, const Json &prefab)
{
    try {
        prefabs.add(name, toComponents(prefab));
    } catch (const std::exception &error) {
        std::cerr << "Warning: prefab '" << name << "' ignored: " << error.what() << '\n';
    }
}

} // namespace

void registerGameComponents(Prefabs &prefabs)
{
    prefabs.registerComponent<components::Kind>("Kind", readKind);
    prefabs.registerComponent<components::Hitbox>("Hitbox", readHitbox);
    prefabs.registerComponent<components::Health>(
        "Health", [](const Json &json) { return components::Health{json.value("hp", 1)}; });
    prefabs.registerComponent<components::Velocity>(
        "Velocity", [](const Json &json) { return components::Velocity{json.value("x", 0.f), json.value("y", 0.f)}; });
    prefabs.registerComponent<components::MovementPattern>("MovementPattern", readMovementPattern);
    prefabs.registerComponent<components::EnemyAttack>("EnemyAttack", readEnemyAttack);
    prefabs.registerComponent<components::EnemyTraits>("EnemyTraits", [](const Json &json) {
        return components::EnemyTraits{json.value<std::uint32_t>("score", 100), json.value("is_boss", false)};
    });
    prefabs.registerComponent<components::MissileDamage>(
        "MissileDamage", [](const Json &json) { return components::MissileDamage{json.value("damage", 1)}; });
    prefabs.registerComponent<components::BonusEffect>("BonusEffect", [](const Json &json) {
        return components::BonusEffect{json.value("weapon", std::string()), json.value("lives", 0)};
    });
    prefabs.registerComponent<components::PlayerControl>(
        "PlayerControl", [](const Json &json) { return components::PlayerControl{json.value("speed", 200.f)}; });
}

void loadPrefabs(Prefabs &prefabs, const std::string &filepath)
{
    std::ifstream file(filepath);
    if (!file) {
        std::cerr << "Warning: could not open " << filepath << '\n';
        return;
    }
    try {
        const Json json = Json::parse(file);
        for (const auto &[name, prefab] : json.at("prefabs").items())
            addPrefab(prefabs, name, prefab);
    } catch (const std::exception &error) {
        std::cerr << "Warning: invalid " << filepath << ": " << error.what() << '\n';
    }
}

} // namespace rtype::server
