#pragma once

#include "engine/ecs/Prefabs.hpp"

#include <nlohmann/json.hpp>

#include <string>

namespace brickrush {

// Registers the BrickRush components (Box, Color, Paddle, Ball, Brick) and adds the prefabs of the file.
// Throws std::runtime_error when the file cannot be read: the game cannot run without its bricks.
void loadPrefabs(engine::ecs::Prefabs<nlohmann::json> &prefabs, const std::string &path);

// Reads a whole JSON file. Throws std::runtime_error when it cannot be read.
nlohmann::json readJson(const std::string &path);

} // namespace brickrush
