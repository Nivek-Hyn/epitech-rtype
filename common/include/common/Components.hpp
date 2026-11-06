#pragma once

#include "common/Protocol.hpp"

#include <cstdint>
#include <string>
#include <vector>

// Components used by both the server and the client.
// Server-only components (Health, Hitbox, AI...) go in server/, client-only ones (Sprite, Animation...) in client/.
namespace rtype::components {

struct Position {
    float x = 0.f;
    float y = 0.f;
};

// World units per second.
struct Velocity {
    float x = 0.f;
    float y = 0.f;
};

// Id shared by the server and every client to refer to the same entity over the network.
// The local ECS Entity index is different on each machine and must never be sent.
struct NetworkId {
    std::uint32_t id = 0;
};

// What the entity is, so the client knows how to display it.
struct Kind {
    protocol::EntityType type = protocol::EntityType::Player;
    std::uint8_t variant = 0; // player index (0-3) for players, subtype for enemies
};

struct Health {
    int hp = 3;
};

struct MovementPattern {
    std::string type = "straight"; // straight, sine_wave, zigzag, dive, etc.
    float amplitude = 0.f; // 0: default of the pattern
    float frequency = 0.f; // 0: default of the pattern
    float baseSpeed = 100.f; // horizontal speed used by the patterns that drive vel.x
    float holdX = 0.f; // boss only: where it stops (0: keeps moving)
    float age = 0.f; // seconds since spawn
    float heldTravel = 0.f; // distance it would have travelled while stopped
    bool ejected = false; // black_hole_orbit: thrown out by the black hole
    float bounceSpeed = 0.f; // martian_bounce: set once, so that every jump has the same height
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
    std::uint8_t bulletVariant = 0; // Pour choisir le sprite du tir
    std::string bulletPattern = "straight"; // straight, sine_wave, zigzag, dive, etc.
    float offsetY = 0.f;
};

struct EnemyAttack {
    std::vector<AttackEntry> attacks;

    EnemyAttack() = default;
    EnemyAttack(std::vector<AttackEntry> atts) : attacks(std::move(atts)) {}

    // Constructeur de compatibilité pour une attaque unique
    EnemyAttack(float shootTimer, float fireRate, float bulletSpeed, std::uint8_t bulletVariant = 0, std::string bulletPattern = "straight", float offsetY = 0.f)
    {
        attacks.push_back(AttackEntry{shootTimer, fireRate, bulletSpeed, bulletVariant, std::move(bulletPattern), offsetY});
    }
};

} // namespace rtype::components
