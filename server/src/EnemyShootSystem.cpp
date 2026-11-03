#include "EnemyShootSystem.hpp"
#include "common/Components.hpp"
#include <cmath>

namespace rtype::server {

void updateEnemyShooting(engine::ecs::Registry &registry, const std::function<engine::ecs::Entity(protocol::EntityType, std::uint8_t, float, float)> &spawnFunc, float dt)
{
    //Détection des ennemis qui doivent tirer
    registry.each<rtype::components::Position, rtype::components::Kind, rtype::components::EnemyAttack>([&registry, &spawnFunc, dt](engine::ecs::Entity /*entity*/, const rtype::components::Position &pos, const rtype::components::Kind &kind, rtype::components::EnemyAttack &attack) {
            
            if (kind.type != protocol::EntityType::Enemy)
                return;

            attack.shootTimer += dt;
            if (attack.shootTimer >= attack.fireRate) {
                attack.shootTimer = 0.f;

                engine::ecs::Entity bullet = spawnFunc(protocol::EntityType::EnemyMissile, attack.bulletVariant, pos.x - 30.0f, pos.y);
                registry.emplace<rtype::components::Velocity>(bullet, -attack.bulletSpeed, 0.0f);
                if (!attack.bulletPattern.empty() && attack.bulletPattern != "straight") {
                    registry.emplace<rtype::components::MovementPattern>(bullet, rtype::components::MovementPattern{attack.bulletPattern});
                }
            }
        });

    //Traitement des trajectoires spéciales des tirs ennemis (ex: shoot zigzag)
    registry.each<rtype::components::Position, rtype::components::Velocity, rtype::components::Kind, rtype::components::MovementPattern>([](engine::ecs::Entity /*entity*/, const rtype::components::Position &pos, rtype::components::Velocity &vel, const rtype::components::Kind &kind, const rtype::components::MovementPattern &pattern) {
            if (kind.type != protocol::EntityType::EnemyMissile)
                return;

            if (pattern.type == "zigzag") {
                vel.y = (std::sin(pos.x * 0.02f) >= 0.f ? 180.f : -180.f);
            } else if (pattern.type == "sine_wave") {
                vel.y = std::sin(pos.x * 0.02f) * 180.f;
            }
        });
}

} // namespace rtype::server