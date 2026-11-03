#pragma once

#include "engine/ecs/Registry.hpp"
#include "common/Protocol.hpp"
#include <functional>

namespace rtype::server {

void updateEnemyShooting(
    engine::ecs::Registry &registry,
    const std::function<engine::ecs::Entity(protocol::EntityType, std::uint8_t, float, float)> &spawnFunc,
    float dt
);

} // namespace rtype::server
