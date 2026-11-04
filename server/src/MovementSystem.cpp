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
                float amp = pattern.amplitude > 0.f ? pattern.amplitude : 150.f;
                float freq = pattern.frequency > 0.f ? pattern.frequency : 0.02f;
                vel.y = std::sin(pos.x * freq) * amp;
            }
            else if (pattern.type == "straight") {
                // 2. Tout droit (pas de mouvement vertical)
                vel.y = 0.f;
            }
            else if (pattern.type == "zigzag") {
                // 3. Mouvement de va-et-vient plus brusque
                float amp = pattern.amplitude > 0.f ? pattern.amplitude : 100.f;
                float freq = pattern.frequency > 0.f ? pattern.frequency : 0.05f;
                vel.y = std::cos(pos.x * freq) * amp;
            }
            else if (pattern.type == "dive") {
                // 4. Plongée vers le bas en diagonale
                float amp = pattern.amplitude > 0.f ? pattern.amplitude : 80.f;
                vel.y = amp;
            }
            else if (pattern.type == "back_and_forth") {
                // 5. Avance et recule (modifie la vitesse horizontale X de manière oscillante)
                float amp = pattern.amplitude > 0.f ? pattern.amplitude : 50.f;
                float freq = pattern.frequency > 0.f ? pattern.frequency : 3.0f;
                vel.x = -100.f + std::sin(timeElapsed * freq) * amp;
                vel.y = 0.f;
            }
            else if (pattern.type == "orbital") {
                // 6. Mouvement orbital de satellite (oscillation circulaire orbitale)
                float amp = pattern.amplitude > 0.f ? pattern.amplitude : 120.f;
                float freq = pattern.frequency > 0.f ? pattern.frequency : 2.0f;
                vel.y = std::sin(timeElapsed * freq) * amp;
                vel.x = -90.f + std::cos(timeElapsed * freq) * (amp * 0.4f);
            }
            else if (pattern.type == "gravity_dive") {
                // 7. Plongée gravitationnelle martienne lourde
                float amp = pattern.amplitude > 0.f ? pattern.amplitude : 160.f;
                vel.y = amp;
            }
            else if (pattern.type == "parabolic") {
                // 8. Saut / arc parabolique sous gravité martienne
                float amp = pattern.amplitude > 0.f ? pattern.amplitude : 150.f;
                float freq = pattern.frequency > 0.f ? pattern.frequency : 0.008f;
                vel.y = std::sin(pos.x * freq) * amp;
            }
            else if (pattern.type == "vortex_spiral") {
                // 9. Aspiration spirale du trou noir vers le centre de l'espace (y ~ 540)
                float amp = pattern.amplitude > 0.f ? pattern.amplitude : 140.f;
                float dy = 540.f - pos.y;
                vel.y = std::sin(timeElapsed * 3.5f) * amp + (dy * 0.6f);
            }
            else if (pattern.type == "quantum_erratic") {
                // 10. Distorsion quantique erratique imprévisible
                float amp = pattern.amplitude > 0.f ? pattern.amplitude : 130.f;
                vel.y = (std::sin(pos.x * 0.03f) + std::cos(timeElapsed * 4.0f)) * (amp * 0.6f);
            }
        });
}

} // namespace rtype::server