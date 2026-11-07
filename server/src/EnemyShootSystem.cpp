#include "EnemyShootSystem.hpp"
#include "common/Components.hpp"
#include "common/Constants.hpp"
#include <cmath>

namespace rtype::server {

void updateEnemyShooting(
    engine::ecs::Registry &registry,
    const std::function<engine::ecs::Entity(protocol::EntityType, std::uint8_t, float, float)> &spawnFunc, float dt)
{
    struct PendingShot {
        std::uint8_t variant;
        float x;
        float y;
        float speed;
        std::string pattern;
    };
    std::vector<PendingShot> shots;

    registry.each<rtype::components::Position, rtype::components::Kind, rtype::components::EnemyAttack>(
        [&shots, dt](engine::ecs::Entity /*entity*/, const rtype::components::Position &pos,
                     const rtype::components::Kind &kind, rtype::components::EnemyAttack &enemyAttack) {
            if (kind.type != protocol::EntityType::Enemy)
                return;

            for (auto &attack : enemyAttack.attacks) {
                attack.shootTimer += dt;
                if (attack.shootTimer >= attack.fireRate) {
                    attack.shootTimer = 0.f;
                    shots.push_back({attack.bulletVariant, pos.x - 30.0f, pos.y + attack.offsetY, attack.bulletSpeed,
                                     attack.bulletPattern});
                }
            }
        });

    for (const auto &shot : shots) {
        engine::ecs::Entity bullet = spawnFunc(protocol::EntityType::EnemyMissile, shot.variant, shot.x, shot.y);
        registry.emplace<rtype::components::Velocity>(bullet, -shot.speed, 0.0f);
        if (!shot.pattern.empty() && shot.pattern != "straight") {
            registry.emplace<rtype::components::MovementPattern>(bullet,
                                                                 rtype::components::MovementPattern{shot.pattern});
        }
    }

    // Traitement des trajectoires spéciales des tirs ennemis (ex: shoot zigzag)
    registry.each<rtype::components::Position, rtype::components::Velocity, rtype::components::Kind,
                  rtype::components::MovementPattern>(
        [dt](engine::ecs::Entity /*entity*/, const rtype::components::Position &pos, rtype::components::Velocity &vel,
             const rtype::components::Kind &kind, const rtype::components::MovementPattern &pattern) {
            if (kind.type != protocol::EntityType::EnemyMissile)
                return;

            if (pattern.type == "zigzag") {
                vel.y = (std::sin(pos.x * 0.02f) >= 0.f ? 180.f : -180.f);
            } else if (pattern.type == "sine_wave") {
                vel.y = std::sin(pos.x * 0.02f) * 180.f;
            } else if (pattern.type == "gravity_well") {
                // Le tir se courbe vers la ligne du trou noir
                vel.y += (WorldHeight / 2.f - pos.y) * 1.5f * dt;
            }
        });
}

} // namespace rtype::server