#include "common/Components.hpp"
#include "engine/ecs/Registry.hpp"
#include "engine/systems/Movement.hpp"

#include <gtest/gtest.h>

TEST(Components, SharedComponentsWorkWithTheGenericMovementSystem)
{
    using namespace rtype::components;

    engine::ecs::Registry registry;
    const auto ship = registry.spawn();
    registry.emplace<Position>(ship, 100.f, 300.f);
    registry.emplace<Velocity>(ship, 200.f, -100.f);
    registry.emplace<NetworkId>(ship, 42u);
    registry.emplace<Kind>(ship, rtype::protocol::EntityType::Player, 2);

    // A stationary object can have a position without a velocity.
    const auto obstacle = registry.spawn();
    registry.emplace<Position>(obstacle, 500.f, 400.f);
    registry.addSystem(engine::systems::movement<Position, Velocity>);
    registry.update(0.25f);

    const auto *position = registry.get<Position>(ship);
    ASSERT_NE(position, nullptr);
    EXPECT_FLOAT_EQ(position->x, 150.f);
    EXPECT_FLOAT_EQ(position->y, 275.f);
    EXPECT_FLOAT_EQ(registry.get<Position>(obstacle)->x, 500.f);
    EXPECT_FLOAT_EQ(registry.get<Position>(obstacle)->y, 400.f);

    // Updating movement must preserve the identity and display category.
    EXPECT_EQ(registry.get<NetworkId>(ship)->id, 42u);
    EXPECT_EQ(registry.get<Kind>(ship)->type, rtype::protocol::EntityType::Player);
    EXPECT_EQ(registry.get<Kind>(ship)->variant, 2);
}
