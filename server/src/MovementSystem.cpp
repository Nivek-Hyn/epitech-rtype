#include "MovementSystem.hpp"
#include "common/Components.hpp"
#include <cmath>

namespace rtype::server {

void updateEnemyMovements(engine::ecs::Registry &registry, float dt)
{
    static float timeElapsed = 0.f;
    timeElapsed += dt;

    // On applique le mouvement selon le pattern stocké dans le composant
    registry.each<rtype::components::Position, rtype::components::Velocity, rtype::components::Kind, rtype::components::MovementPattern>([](engine::ecs::Entity /*entity*/, const rtype::components::Position &pos, rtype::components::Velocity &vel, const rtype::components::Kind &kind, const rtype::components::MovementPattern &pattern) {
            if (kind.type != protocol::EntityType::Enemy)
                return;
            
            if (pattern.type == "sine_wave") {
                // 1. Mouvement sinusoïdal classique (en vagues verticales)
                vel.y = std::sin(pos.x * 0.02f) * 150.f;
            }
            else if (pattern.type == "straight") {
                // 2. Tout droit (pas de mouvement vertical)
                vel.y = 0.f;
            }
            else if (pattern.type == "zigzag") {
                // 3. Mouvement de va-et-vient plus brusque
                vel.y = std::cos(pos.x * 0.05f) * 100.f;
            }
            else if (pattern.type == "dive") {
                // 4. Plongée vers le bas en diagonale
                vel.y = 80.f;
            }
            else if (pattern.type == "back_and_forth") {
                // 5. Avance et recule (modifie la vitesse horizontale X de manière oscillante)
                // L'ennemi avance vers la gauche tout en faisant des mini-pauses ou en reculant un peu
                vel.x = -100.f + std::sin(timeElapsed * 3.0f) * 50.f;
                vel.y = 0.f;
            }
        });
}

} // namespace rtype::server