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
    std::uint8_t bulletVariant = 0;         // Pour choisir le sprite du tir
    std::string bulletPattern = "straight"; // straight, sine_wave, zigzag, dive, etc.
    float offsetY = 0.f;
};

struct EnemyAttack {
    std::vector<AttackEntry> attacks;

    EnemyAttack() = default;
    EnemyAttack(std::vector<AttackEntry> atts) : attacks(std::move(atts)) {}

    // Constructeur de compatibilité pour une attaque unique
    EnemyAttack(float shootTimer, float fireRate, float bulletSpeed, std::uint8_t bulletVariant = 0,
                std::string bulletPattern = "straight", float offsetY = 0.f)
    {
        attacks.push_back(
            AttackEntry{shootTimer, fireRate, bulletSpeed, bulletVariant, std::move(bulletPattern), offsetY});
    }
};

} // namespace rtype::components
