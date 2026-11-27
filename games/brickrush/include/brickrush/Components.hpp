#pragma once

#include "engine/ecs/Entity.hpp"

// Brick Rush components. They belong to this game only: the engine stores them without knowing them.

namespace brickrush {

struct Position {
    float x = 0.f;
    float y = 0.f;
};

// World units per second, read by engine::systems::movement.
struct Velocity {
    float x = 0.f;
    float y = 0.f;
};

// Size of the rectangle, used for collisions and rendering.
struct Box {
    float width = 0.f;
    float height = 0.f;
};

struct Color {
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
};

struct Paddle {
    float speed = 600.f;
};

// The ball stays on the paddle until it is launched.
struct Ball {
    bool attached = true;
    float speed = 450.f;
};

// hits = shots left before it breaks; 0 means it never breaks.
struct Brick {
    int hits = 1;
    int points = 10;
};

// Events sent by the collision system and handled after all systems have run.
struct BrickHit {
    engine::ecs::Entity brick = engine::ecs::NullEntity;
};
struct BallLost {};

} // namespace brickrush
