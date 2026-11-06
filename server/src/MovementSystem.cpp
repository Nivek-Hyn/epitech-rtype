#include "MovementSystem.hpp"
#include "common/Components.hpp"
#include "common/Constants.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

namespace rtype::server {

namespace {

using rtype::components::MovementPattern;

constexpr float MinBounceHeight = 50.f;

// Singularity: the black hole is at the center of the screen. The screen being wider than
// high, orbits are flattened into ellipses so that they stay visible.
constexpr float BlackHoleX = WorldWidth / 2.f;
constexpr float BlackHoleY = WorldHeight / 2.f;
constexpr float OrbitFlattening = 0.5f;
constexpr float BlackHoleCaptureRadius = 120.f;
constexpr float OrbitInwardRatio = 0.6f; // part of the speed used to fall toward the hole
constexpr float EjectionSpeedRatio = 3.f;

// What a movement pattern needs to decide the velocity of one enemy for this tick.
struct Motion {
    const rtype::components::Position &position;
    rtype::components::Velocity &velocity;
    MovementPattern &pattern;
    float dt;
    float time;    // age of the enemy: its movement does not depend on how long the server has run
    float x;       // horizontal position, still advancing while a boss holds its position
    float bottomY; // lowest position keeping the whole sprite on screen (also the ground of Mars)
};

// The value set in the JSON, or the pattern's default when it is 0.
float orDefault(float value, float fallback)
{
    return value > 0.f ? value : fallback;
}

void straight(Motion &m)
{
    m.velocity.y = 0.f;
}

void sineWave(Motion &m)
{
    m.velocity.y = std::sin(m.x * orDefault(m.pattern.frequency, 0.02f)) * orDefault(m.pattern.amplitude, 150.f);
}

void zigzag(Motion &m)
{
    m.velocity.y = std::cos(m.x * orDefault(m.pattern.frequency, 0.05f)) * orDefault(m.pattern.amplitude, 100.f);
}

void dive(Motion &m)
{
    m.velocity.y = orDefault(m.pattern.amplitude, 80.f);
}

void backAndForth(Motion &m)
{
    const float amplitude = orDefault(m.pattern.amplitude, 50.f);
    m.velocity.x = -m.pattern.baseSpeed + std::sin(m.time * orDefault(m.pattern.frequency, 3.0f)) * amplitude;
    m.velocity.y = 0.f;
}

// Satellite: circular oscillation along its way.
void orbital(Motion &m)
{
    const float amplitude = orDefault(m.pattern.amplitude, 120.f);
    const float frequency = orDefault(m.pattern.frequency, 2.0f);
    m.velocity.y = std::sin(m.time * frequency) * amplitude;
    m.velocity.x = -m.pattern.baseSpeed + std::cos(m.time * frequency) * (amplitude * 0.4f);
}

// Flies flat, then falls faster and faster (amplitude = gravity, frequency = seconds before the fall).
void gravityDive(Motion &m)
{
    const float gravity = orDefault(m.pattern.amplitude, 300.f);
    m.velocity.y = m.time < orDefault(m.pattern.frequency, 5.f) ? 0.f : m.velocity.y + gravity * m.dt;
}

// Astronaut jumps: slow fall, bounce on the ground, back up to the spawn height (amplitude = gravity).
void martianBounce(Motion &m)
{
    const float gravity = orDefault(m.pattern.amplitude, 150.f);
    if (m.pattern.bounceSpeed <= 0.f)
        m.pattern.bounceSpeed = std::sqrt(2.f * gravity * std::max(m.bottomY - m.position.y, MinBounceHeight));
    m.velocity.y += gravity * m.dt;
    if (m.position.y >= m.bottomY && m.velocity.y > 0.f)
        m.velocity.y = -m.pattern.bounceSpeed;
}

// Keeps crossing the line of the black hole and coming back (amplitude = strength of the pull).
void gravityPull(Motion &m)
{
    m.velocity.y += (BlackHoleY - m.position.y) * orDefault(m.pattern.amplitude, 1.f) * m.dt;
}

// Spirals around the black hole, then is thrown to the left (amplitude = rotation speed).
void blackHoleOrbit(Motion &m)
{
    if (!m.pattern.ejected) {
        const float orbitSpeed = orDefault(m.pattern.amplitude, 220.f);
        const float inwardSpeed = m.pattern.baseSpeed * OrbitInwardRatio;
        const float toRight = m.position.x - BlackHoleX;
        const float toBottom = (m.position.y - BlackHoleY) / OrbitFlattening;
        const float radius = std::hypot(toRight, toBottom);
        if (radius < BlackHoleCaptureRadius) {
            m.pattern.ejected = true;
        } else {
            m.velocity.x = (-toBottom * orbitSpeed - toRight * inwardSpeed) / radius;
            m.velocity.y = (toRight * orbitSpeed - toBottom * inwardSpeed) / radius * OrbitFlattening;
        }
    }
    if (m.pattern.ejected) {
        m.velocity.x = -m.pattern.baseSpeed * EjectionSpeedRatio;
        m.velocity.y = 0.f;
    }
}

// Jump along a parabolic arc under the gravity of Mars.
void parabolic(Motion &m)
{
    m.velocity.y = std::sin(m.x * orDefault(m.pattern.frequency, 0.008f)) * orDefault(m.pattern.amplitude, 150.f);
}

// Sucked in a spiral toward the center line of the screen.
void vortexSpiral(Motion &m)
{
    const float amplitude = orDefault(m.pattern.amplitude, 140.f);
    m.velocity.y = std::sin(m.time * 3.5f) * amplitude + (BlackHoleY - m.position.y) * 0.6f;
}

// Unpredictable quantum distortion.
void quantumErratic(Motion &m)
{
    const float amplitude = orDefault(m.pattern.amplitude, 130.f);
    m.velocity.y = (std::sin(m.x * 0.03f) + std::cos(m.time * 4.0f)) * (amplitude * 0.6f);
}

// The "type" of a MovementPattern in entities.json, and the function that applies it.
const std::unordered_map<std::string, void (*)(Motion &)> &patterns()
{
    static const std::unordered_map<std::string, void (*)(Motion &)> table = {
        {"straight", straight},
        {"sine_wave", sineWave},
        {"zigzag", zigzag},
        {"dive", dive},
        {"back_and_forth", backAndForth},
        {"orbital", orbital},
        {"gravity_dive", gravityDive},
        {"martian_bounce", martianBounce},
        {"gravity_pull", gravityPull},
        {"black_hole_orbit", blackHoleOrbit},
        {"parabolic", parabolic},
        {"vortex_spiral", vortexSpiral},
        {"quantum_erratic", quantumErratic},
    };
    return table;
}

// A boss stops at its fighting position and never leaves the screen.
void holdBoss(Motion &m, bool holding, float topY)
{
    if (m.pattern.holdX <= 0.f)
        return;
    if (holding)
        m.velocity.x = 0.f;
    if ((m.position.y <= topY && m.velocity.y < 0.f) || (m.position.y >= m.bottomY && m.velocity.y > 0.f))
        m.velocity.y = 0.f;
}

void moveEnemy(const rtype::components::Position &position, rtype::components::Velocity &velocity,
               MovementPattern &pattern, const rtype::components::Hitbox &box, float dt)
{
    pattern.age += dt;
    // A stopped boss keeps advancing "virtually", so the patterns based on x keep oscillating.
    const bool holding = pattern.holdX > 0.f && position.x <= pattern.holdX;
    if (holding)
        pattern.heldTravel += pattern.baseSpeed * dt;

    Motion motion{position,
                  velocity,
                  pattern,
                  dt,
                  pattern.age,
                  position.x - pattern.heldTravel,
                  WorldHeight - box.offsetY - box.height};
    if (const auto apply = patterns().find(pattern.type); apply != patterns().end())
        apply->second(motion);
    holdBoss(motion, holding, -box.offsetY);
}

} // namespace

void updateEnemyMovements(engine::ecs::Registry &registry, float dt)
{
    registry.each<rtype::components::Position, rtype::components::Velocity, rtype::components::Kind, MovementPattern>(
        [dt, &registry](engine::ecs::Entity entity, const rtype::components::Position &position,
                        rtype::components::Velocity &velocity, const rtype::components::Kind &kind,
                        MovementPattern &pattern) {
            if (kind.type != protocol::EntityType::Enemy)
                return;
            const auto *hitbox = registry.get<rtype::components::Hitbox>(entity);
            moveEnemy(position, velocity, pattern, hitbox ? *hitbox : rtype::components::Hitbox{}, dt);
        });
}

} // namespace rtype::server
