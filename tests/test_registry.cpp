#include "engine/ecs/Registry.hpp"

#include <gtest/gtest.h>

using engine::ecs::Entity;
using engine::ecs::Registry;

namespace {
struct Position {
    float x = 0.f;
    float y = 0.f;
};
struct Velocity {
    float x = 0.f;
    float y = 0.f;
};
} // namespace

TEST(Registry, SpawnGivesDistinctIds)
{
    Registry registry;
    const Entity first = registry.spawn();
    const Entity second = registry.spawn();
    EXPECT_NE(first, second);
}

TEST(Registry, SpawnedEntitiesAreAliveAndUnknownIdsAreNot)
{
    Registry registry;
    EXPECT_FALSE(registry.alive(0));
    const Entity entity = registry.spawn();
    EXPECT_TRUE(registry.alive(entity));
    EXPECT_FALSE(registry.alive(100));
    EXPECT_FALSE(registry.alive(engine::ecs::NullEntity));
}

TEST(Registry, KillPreservesOtherEntitiesAndReusesOnlyTheFreedId)
{
    Registry registry;
    const Entity first = registry.spawn();
    const Entity second = registry.spawn();
    registry.kill(first);
    EXPECT_FALSE(registry.alive(first));
    EXPECT_TRUE(registry.alive(second));
    const Entity replacement = registry.spawn();
    EXPECT_EQ(replacement, first);
    EXPECT_TRUE(registry.alive(replacement));
    const Entity next = registry.spawn();
    EXPECT_NE(next, first);
    EXPECT_NE(next, second);
}

TEST(Registry, KillingDeadOrUnknownEntitiesDoesNotDuplicateFreeIds)
{
    Registry registry;
    const Entity entity = registry.spawn();
    registry.kill(entity);
    registry.kill(entity);
    registry.kill(100);
    registry.kill(engine::ecs::NullEntity);
    EXPECT_EQ(registry.spawn(), entity);
    EXPECT_NE(registry.spawn(), entity);
}

TEST(Registry, KillRemovesComponentsAndReusesId)
{
    Registry registry;
    const Entity ship = registry.spawn();
    registry.emplace<Position>(ship, 1.f, 2.f);
    registry.kill(ship);
    EXPECT_FALSE(registry.alive(ship));
    EXPECT_FALSE(registry.has<Position>(ship));
    EXPECT_EQ(registry.spawn(), ship);
}

TEST(Registry, GetReturnsTheComponent)
{
    Registry registry;
    const Entity ship = registry.spawn();
    registry.emplace<Position>(ship, 3.f, 4.f);
    ASSERT_NE(registry.get<Position>(ship), nullptr);
    EXPECT_FLOAT_EQ(registry.get<Position>(ship)->x, 3.f);
    EXPECT_EQ(registry.get<Velocity>(ship), nullptr);
}

TEST(Registry, EachVisitsOnlyEntitiesWithAllComponents)
{
    Registry registry;
    const Entity moving = registry.spawn();
    const Entity standing = registry.spawn();
    registry.emplace<Position>(moving);
    registry.emplace<Velocity>(moving, 1.f, 0.f);
    registry.emplace<Position>(standing);

    int visitedCount = 0;
    registry.each<Position, Velocity>([&](Entity entity, Position &, Velocity &) {
        EXPECT_EQ(entity, moving);
        ++visitedCount;
    });
    EXPECT_EQ(visitedCount, 1);
}

TEST(Registry, UpdateRunsSystems)
{
    Registry registry;
    const Entity ship = registry.spawn();
    registry.emplace<Position>(ship);
    registry.emplace<Velocity>(ship, 10.f, 0.f);
    registry.addSystem([](Registry &reg, float dt) {
        reg.each<Position, Velocity>(
            [dt](Entity, Position &position, Velocity &velocity) { position.x += velocity.x * dt; });
    });
    registry.update(0.5f);
    EXPECT_FLOAT_EQ(registry.get<Position>(ship)->x, 5.f);
}
