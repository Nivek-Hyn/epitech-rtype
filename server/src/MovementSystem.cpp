#include "MovementSystem.hpp"
#include "common/Components.hpp"
#include "common/Constants.hpp"
#include <algorithm>
#include <cmath>

namespace rtype::server {

namespace {

constexpr float BossHeight = 72.f;

// Mars: the ground an enemy bounces on.
constexpr float MarsGroundY = WorldHeight - BossHeight;
constexpr float MinBounceHeight = 50.f;

// Singularity: the black hole is at the center of the screen. The screen being wider than
// high, orbits are flattened into ellipses so that they stay visible.
constexpr float BlackHoleX = WorldWidth / 2.f;
constexpr float BlackHoleY = WorldHeight / 2.f;
constexpr float OrbitFlattening = 0.5f;
constexpr float BlackHoleCaptureRadius = 120.f;
constexpr float OrbitInwardRatio = 0.6f; // part of the speed used to fall toward the hole
constexpr float EjectionSpeedRatio = 3.f;

} // namespace

void updateEnemyMovements(engine::ecs::Registry &registry, float dt)
{
    // On applique le mouvement selon le pattern stocké dans le composant
    registry.each<rtype::components::Position, rtype::components::Velocity, rtype::components::Kind, rtype::components::MovementPattern>([dt](engine::ecs::Entity /*entity*/, const rtype::components::Position &pos, rtype::components::Velocity &vel, const rtype::components::Kind &kind, rtype::components::MovementPattern &pattern) {
            if (kind.type != protocol::EntityType::Enemy)
                return;

            // Chaque ennemi a sa propre horloge : son mouvement ne dépend pas de
            // la durée d'ouverture du serveur.
            pattern.age += dt;
            const float time = pattern.age;

            // Un boss arrêté continue d'avancer "virtuellement", pour que les
            // patterns basés sur X continuent d'osciller.
            const bool holding = pattern.holdX > 0.f && pos.x <= pattern.holdX;
            if (holding)
                pattern.heldTravel += pattern.baseSpeed * dt;
            const float x = pos.x - pattern.heldTravel;

            if (pattern.type == "sine_wave") {
                // 1. Mouvement sinusoïdal classique (en vagues verticales)
                float amp = pattern.amplitude > 0.f ? pattern.amplitude : 150.f;
                float freq = pattern.frequency > 0.f ? pattern.frequency : 0.02f;
                vel.y = std::sin(x * freq) * amp;
            }
            else if (pattern.type == "straight") {
                // 2. Tout droit (pas de mouvement vertical)
                vel.y = 0.f;
            }
            else if (pattern.type == "zigzag") {
                // 3. Mouvement de va-et-vient plus brusque
                float amp = pattern.amplitude > 0.f ? pattern.amplitude : 100.f;
                float freq = pattern.frequency > 0.f ? pattern.frequency : 0.05f;
                vel.y = std::cos(x * freq) * amp;
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
                vel.x = -pattern.baseSpeed + std::sin(time * freq) * amp;
                vel.y = 0.f;
            }
            else if (pattern.type == "orbital") {
                // 6. Mouvement orbital de satellite (oscillation circulaire orbitale)
                float amp = pattern.amplitude > 0.f ? pattern.amplitude : 120.f;
                float freq = pattern.frequency > 0.f ? pattern.frequency : 2.0f;
                vel.y = std::sin(time * freq) * amp;
                vel.x = -pattern.baseSpeed + std::cos(time * freq) * (amp * 0.4f);
            }
            else if (pattern.type == "gravity_dive") {
                // 7. Vol à plat, puis chute de plus en plus rapide (amplitude = gravité, frequency = secondes avant la chute)
                float gravity = pattern.amplitude > 0.f ? pattern.amplitude : 300.f;
                float delay = pattern.frequency > 0.f ? pattern.frequency : 5.f;
                vel.y = time < delay ? 0.f : vel.y + gravity * dt;
            }
            else if (pattern.type == "martian_bounce") {
                // Bonds d'astronaute : chute lente, rebond au sol, remontée jusqu'à spawn_y (amplitude = gravité)
                float gravity = pattern.amplitude > 0.f ? pattern.amplitude : 150.f;
                if (pattern.bounceSpeed <= 0.f)
                    pattern.bounceSpeed = std::sqrt(2.f * gravity * std::max(MarsGroundY - pos.y, MinBounceHeight));
                vel.y += gravity * dt;
                if (pos.y >= MarsGroundY && vel.y > 0.f)
                    vel.y = -pattern.bounceSpeed;
            }
            else if (pattern.type == "gravity_pull") {
                // Traverse la ligne du trou noir et y revient sans cesse (amplitude = force d'attraction)
                float strength = pattern.amplitude > 0.f ? pattern.amplitude : 1.f;
                vel.y += (BlackHoleY - pos.y) * strength * dt;
            }
            else if (pattern.type == "black_hole_orbit") {
                // Spirale autour du trou noir, puis éjection vers la gauche (amplitude = vitesse de rotation)
                if (!pattern.ejected) {
                    const float orbitSpeed = pattern.amplitude > 0.f ? pattern.amplitude : 220.f;
                    const float inwardSpeed = pattern.baseSpeed * OrbitInwardRatio;
                    const float toRight = pos.x - BlackHoleX;
                    const float toBottom = (pos.y - BlackHoleY) / OrbitFlattening;
                    const float radius = std::hypot(toRight, toBottom);
                    if (radius < BlackHoleCaptureRadius) {
                        pattern.ejected = true;
                    } else {
                        vel.x = (-toBottom * orbitSpeed - toRight * inwardSpeed) / radius;
                        vel.y = (toRight * orbitSpeed - toBottom * inwardSpeed) / radius * OrbitFlattening;
                    }
                }
                if (pattern.ejected) {
                    vel.x = -pattern.baseSpeed * EjectionSpeedRatio;
                    vel.y = 0.f;
                }
            }
            else if (pattern.type == "parabolic") {
                // 8. Saut / arc parabolique sous gravité martienne
                float amp = pattern.amplitude > 0.f ? pattern.amplitude : 150.f;
                float freq = pattern.frequency > 0.f ? pattern.frequency : 0.008f;
                vel.y = std::sin(x * freq) * amp;
            }
            else if (pattern.type == "vortex_spiral") {
                // 9. Aspiration spirale du trou noir vers le centre de l'espace (y ~ 540)
                float amp = pattern.amplitude > 0.f ? pattern.amplitude : 140.f;
                float dy = 540.f - pos.y;
                vel.y = std::sin(time * 3.5f) * amp + (dy * 0.6f);
            }
            else if (pattern.type == "quantum_erratic") {
                // 10. Distorsion quantique erratique imprévisible
                float amp = pattern.amplitude > 0.f ? pattern.amplitude : 130.f;
                vel.y = (std::sin(x * 0.03f) + std::cos(time * 4.0f)) * (amp * 0.6f);
            }

            // Un boss s'arrête à sa position de combat et ne quitte jamais l'écran.
            if (pattern.holdX > 0.f) {
                if (holding)
                    vel.x = 0.f;
                if ((pos.y <= 0.f && vel.y < 0.f) || (pos.y >= WorldHeight - BossHeight && vel.y > 0.f))
                    vel.y = 0.f;
            }
        });
}

} // namespace rtype::server
