#include "MovementSystem.hpp"
#include "common/Components.hpp"
#include <cmath>

namespace rtype::server {

void updateEnemyMovements(engine::ecs::Registry &registry, float dt)
{
    static float timeElapsed = 0.f;
    timeElapsed += dt;

    registry.each<rtype::components::Position, rtype::components::Velocity, rtype::components::Kind, rtype::components::MovementPattern>([](engine::ecs::Entity /*entity*/, const rtype::components::Position &pos, rtype::components::Velocity &vel, const rtype::components::Kind &kind, const rtype::components::MovementPattern &pattern) {
            if (kind.type != protocol::EntityType::Enemy)
                return;
            
            if (pattern.type == "sine_wave") {
                vel.y = std::sin(pos.x * 0.02f) * 150.f;
            } else if (pattern.type == "straight") {
                vel.y = 0.f;
            } else if (pattern.type == "zigzag") {
                vel.y = std::cos(pos.x * 0.05f) * 100.f;
            } else if (pattern.type == "dive") {
                vel.y = 80.f;
            } else if (pattern.type == "back_and_forth") {
                vel.x = -100.f + std::sin(timeElapsed * 3.0f) * 50.f;
                vel.y = 0.f;
            }
        });
}

} // namespace rtype::server