#pragma once

#include "engine/ecs/Registry.hpp"

namespace engine::systems {

// Move every entity having both components: position += velocity * timeStepSeconds.
// The game chooses the component types; each needs float members x and y, velocity in units per second.
//
//   registry.addSystem(engine::systems::movement<Position, Velocity>);
template <typename Position, typename Velocity> void movement(ecs::Registry &registry, float timeStepSeconds)
{
    registry.each<Position, Velocity>([timeStepSeconds](ecs::Entity, Position &position, const Velocity &velocity) {
        position.x += velocity.x * timeStepSeconds;
        position.y += velocity.y * timeStepSeconds;
    });
}

} // namespace engine::systems
