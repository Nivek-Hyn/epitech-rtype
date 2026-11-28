#pragma once

#include "common/Components.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Components only the server uses: gameplay data read from the prefabs of assets/config/entities.json.
// The client never sees them; it only receives what the snapshots carry (common/Components.hpp).
namespace rtype::components {

// Hits left before the entity is destroyed.
struct Health {
    int hp = 3;
    int maxHp = 0; // health when it came into play, after the difficulty (0: not known yet)
};

// Movement of an enemy, applied by MovementSystem.cpp; type is one of the names of its pattern table.
struct MovementPattern {
    std::string type = "straight";
    float amplitude = 0.f;
    float frequency = 0.f;
    float baseSpeed = 100.f;
    float holdX = 0.f;
    float age = 0.f;
    float heldTravel = 0.f;
    bool ejected = false;
    float bounceSpeed = 0.f;
};

// Area of the entity that can be hit, relative to its position. Matches the size of its sprite.
// An entity without Hitbox never collides.
struct Hitbox {
    float width = 0.f;
    float height = 0.f;
    float offsetY = 0.f;
};

// Health removed by a missile from what it hits.
struct MissileDamage {
    int damage = 1;
};

// What a player gets by touching a bonus: a weapon (named in weapons.json), lives, or both.
struct BonusEffect {
    std::string weapon;
    int lives = 0;
};

// A ship moved by a player's inputs, in world units per second.
struct PlayerControl {
    float speed = 200.f;
};

// Game rules specific to an enemy type.
struct EnemyTraits {
    std::uint32_t score = 100;
    bool isBoss = false; // a boss survives contact with a player
};

struct AttackEntry {
    float shootTimer = 0.f;
    float fireRate = 2.0f;
    float bulletSpeed = 350.f;
    std::uint8_t bulletVariant = 0;         // sprite of the shot, drawn by the client
    std::string bulletPattern = "straight"; // straight, zigzag, sine_wave or gravity_well
    float offsetY = 0.f;
};

struct EnemyAttack {
    std::vector<AttackEntry> attacks;

    EnemyAttack() = default;
    EnemyAttack(std::vector<AttackEntry> atts) : attacks(std::move(atts)) {}

    // A single attack.
    EnemyAttack(float shootTimer, float fireRate, float bulletSpeed, std::uint8_t bulletVariant = 0,
                std::string bulletPattern = "straight", float offsetY = 0.f)
    {
        attacks.push_back(
            AttackEntry{shootTimer, fireRate, bulletSpeed, bulletVariant, std::move(bulletPattern), offsetY});
    }
};

} // namespace rtype::components
