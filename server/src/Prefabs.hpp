#pragma once

#include "engine/ecs/Prefabs.hpp"

#include <nlohmann/json.hpp>

#include <string>

namespace rtype::server {

// Entity templates read from assets/config/entities.json: each prefab lists its components by name.
using Prefabs = engine::ecs::Prefabs<nlohmann::json>;

// Registers every component a prefab may contain (Kind, Hitbox, Health, Velocity, MovementPattern...).
void registerGameComponents(Prefabs &prefabs);

// Adds the prefabs of the file, replacing those with the same name. Prints a warning and keeps the
// current prefabs when the file is missing or invalid.
void loadPrefabs(Prefabs &prefabs, const std::string &filepath);

} // namespace rtype::server
