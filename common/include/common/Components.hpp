#pragma once

#include "common/Protocol.hpp"

#include <cstdint>
#include <string>

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
};

} // namespace rtype::components
