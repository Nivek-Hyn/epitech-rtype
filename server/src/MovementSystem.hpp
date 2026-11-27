#pragma once

#include "GameComponents.hpp"
#include "engine/ecs/Registry.hpp"

namespace rtype::server {

// Sets the velocity of every enemy from its MovementPattern, once per tick, before the movement system.
void updateEnemyMovements(engine::ecs::Registry &registry, float dt);

} // namespace rtype::server