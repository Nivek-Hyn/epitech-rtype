#pragma once

#include "engine/ecs/Registry.hpp"

namespace engine::systems {

/// @brief System moving every entity having both components: `position += velocity * timeStepSeconds`.
///
/// @code
/// registry.addSystem(engine::systems::movement<Position, Velocity>);
/// @endcode
///
/// @tparam Position Game component with `float x` and `float y`.
/// @tparam Velocity Game component with `float x` and `float y`, in units per second.
template <typename Position, typename Velocity> void movement(ecs::Registry &registry, float timeStepSeconds)
{
    registry.each<Position, Velocity>([timeStepSeconds](ecs::Entity, Position &position, const Velocity &velocity) {
        position.x += velocity.x * timeStepSeconds;
        position.y += velocity.y * timeStepSeconds;
    });
}

} // namespace engine::systems
