#include "engine/systems/Movement.hpp"

#include <gtest/gtest.h>

using engine::ecs::Entity;
using engine::ecs::Registry;

namespace {

// Test-only components: the engine must work with any type having float x and y.
struct Position {
    float x = 0.f;
    float y = 0.f;
};

struct Velocity {
    float x = 0.f;
    float y = 0.f;
};

} // namespace

TEST(Movement, VelocityIsScaledByTheTimeStep)
{
    Registry registry;
    const Entity ship = registry.spawn();
    registry.emplace<Position>(ship, 10.f, 20.f);
    registry.emplace<Velocity>(ship, 100.f, -50.f);

    engine::systems::movement<Position, Velocity>(registry, 0.5f);

    EXPECT_FLOAT_EQ(registry.get<Position>(ship)->x, 60.f);
    EXPECT_FLOAT_EQ(registry.get<Position>(ship)->y, -5.f);
    EXPECT_FLOAT_EQ(registry.get<Velocity>(ship)->x, 100.f);
}

TEST(Movement, EntitiesWithoutVelocityDoNotMove)
{
    Registry registry;
    const Entity wall = registry.spawn();
    registry.emplace<Position>(wall, 5.f, 5.f);

    engine::systems::movement<Position, Velocity>(registry, 1.f);

    EXPECT_FLOAT_EQ(registry.get<Position>(wall)->x, 5.f);
    EXPECT_FLOAT_EQ(registry.get<Position>(wall)->y, 5.f);
}

TEST(Movement, CanBeRegisteredAsASystem)
{
    Registry registry;
    const Entity missile = registry.spawn();
    registry.emplace<Position>(missile);
    registry.emplace<Velocity>(missile, 600.f, 0.f);
    registry.addSystem(engine::systems::movement<Position, Velocity>);

    for (int i = 0; i < 60; i++)
        registry.update(1.f / 60.f);

    EXPECT_NEAR(registry.get<Position>(missile)->x, 600.f, 0.01f);
}
