#include "engine/ecs/Registry.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

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

TEST(Registry, ComponentTypesHaveSeparatePersistentStorages)
{
    Registry registry;
    auto &positions = registry.components<Position>();
    EXPECT_EQ(&positions, &registry.components<Position>());
    const Entity entity = registry.spawn();
    Position &position = registry.emplace<Position>(entity, 3.f, 4.f);
    registry.emplace<Velocity>(entity, 5.f, 6.f);
    EXPECT_EQ(registry.get<Position>(entity), &position);
    EXPECT_FLOAT_EQ(registry.get<Velocity>(entity)->x, 5.f);
    registry.emplace<Position>(entity, 7.f, 8.f);
    EXPECT_FLOAT_EQ(registry.get<Position>(entity)->x, 7.f);
}

TEST(Registry, RemovingOneComponentPreservesOthersAndTheEntity)
{
    Registry registry;
    const Entity entity = registry.spawn();
    registry.remove<Position>(entity);
    registry.emplace<Position>(entity);
    registry.emplace<Velocity>(entity);
    registry.remove<Position>(entity);
    EXPECT_FALSE(registry.has<Position>(entity));
    EXPECT_TRUE(registry.has<Velocity>(entity));
    EXPECT_TRUE(registry.alive(entity));
    registry.remove<Position>(entity);
    registry.remove<Velocity>(engine::ecs::NullEntity);
    EXPECT_TRUE(registry.has<Velocity>(entity));
}

TEST(Registry, ReusedEntityDoesNotInheritAnyOldComponents)
{
    Registry registry;
    const Entity entity = registry.spawn();
    registry.emplace<Position>(entity);
    registry.emplace<Velocity>(entity);
    registry.kill(entity);
    EXPECT_FALSE(registry.components<Position>().has(entity));
    EXPECT_FALSE(registry.components<Velocity>().has(entity));
    EXPECT_EQ(registry.spawn(), entity);
    EXPECT_EQ(registry.get<Position>(entity), nullptr);
    EXPECT_FALSE(registry.has<Velocity>(entity));
}

TEST(Registry, CannotAttachComponentsToDeadOrUnknownEntities)
{
    Registry registry;
    EXPECT_THROW(registry.emplace<Position>(engine::ecs::NullEntity), std::invalid_argument);
    EXPECT_THROW(registry.emplace<Position>(100), std::invalid_argument);
    const Entity entity = registry.spawn();
    registry.kill(entity);
    EXPECT_THROW(registry.emplace<Position>(entity), std::invalid_argument);
    EXPECT_EQ(registry.get<Position>(entity), nullptr);
    EXPECT_FALSE(registry.has<Position>(100));
}

TEST(Registry, ForwardsMoveOnlyComponentsAndDestroysThemOnKill)
{
    Registry registry;
    const Entity entity = registry.spawn();
    auto owner = std::make_shared<int>(42);
    std::weak_ptr<int> observer = owner;
    registry.emplace<std::shared_ptr<int>>(entity, std::move(owner));
    EXPECT_EQ(owner, nullptr);
    EXPECT_FALSE(observer.expired());
    registry.emplace<std::unique_ptr<int>>(entity, std::make_unique<int>(7));
    ASSERT_NE(registry.get<std::unique_ptr<int>>(entity), nullptr);
    EXPECT_EQ(**registry.get<std::unique_ptr<int>>(entity), 7);
    registry.kill(entity);
    EXPECT_TRUE(observer.expired());
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

TEST(Registry, EachModifiesTheStoredComponents)
{
    Registry registry;
    const Entity entity = registry.spawn();
    registry.emplace<Position>(entity, 3.f, 4.f);
    registry.emplace<Velocity>(entity, 2.f, 0.f);
    registry.each<Position, Velocity>([](Entity, Position &position, Velocity &velocity) {
        position.x += velocity.x;
        velocity.y = 1.f;
    });
    EXPECT_FLOAT_EQ(registry.get<Position>(entity)->x, 5.f);
    EXPECT_FLOAT_EQ(registry.get<Velocity>(entity)->y, 1.f);
}

TEST(Registry, EachSkipsDeadEntitiesAndAcceptsASingleComponent)
{
    Registry registry;
    const Entity dead = registry.spawn();
    const Entity live = registry.spawn();
    registry.emplace<Position>(dead);
    registry.emplace<Position>(live);
    registry.kill(dead);
    // Direct storage access can populate a dead slot; each must still skip it.
    registry.components<Position>().emplace(dead);
    int visitedCount = 0;
    registry.each<Position>([&](Entity entity, Position &) {
        EXPECT_EQ(entity, live);
        ++visitedCount;
    });
    EXPECT_EQ(visitedCount, 1);
}

TEST(Registry, EachHandlesEmptyRegistryAndMissingComponentTypes)
{
    Registry registry;
    int visitedCount = 0;
    auto visitor = [&](Entity, Position &, Velocity &) { ++visitedCount; };
    registry.each<Position, Velocity>(visitor);
    const Entity entity = registry.spawn();
    registry.emplace<Position>(entity);
    registry.each<Position, Velocity>(visitor);
    EXPECT_EQ(visitedCount, 0);
}

TEST(Registry, EachWithoutComponentsDoesNotVisitNewlyAppendedEntities)
{
    Registry registry;
    const Entity original = registry.spawn();
    int visitedCount = 0;
    registry.each<>([&](Entity entity) {
        EXPECT_EQ(entity, original);
        ++visitedCount;
        registry.spawn();
    });
    EXPECT_EQ(visitedCount, 1);
}

TEST(Registry, UpdateRunsSystems)
{
    Registry registry;
    const Entity ship = registry.spawn();
    registry.emplace<Position>(ship);
    registry.emplace<Velocity>(ship, 10.f, 0.f);
    registry.addSystem([](Registry &world, float timeStepSeconds) {
        world.each<Position, Velocity>([timeStepSeconds](Entity, Position &position, Velocity &velocity) {
            position.x += velocity.x * timeStepSeconds;
        });
    });
    registry.update(0.5f);
    EXPECT_FLOAT_EQ(registry.get<Position>(ship)->x, 5.f);
}

TEST(Registry, SystemsRunInOrderWithTheSameRegistryAndTimestep)
{
    Registry registry;
    std::vector<int> calls;
    registry.addSystem([&](Registry &world, float timeStepSeconds) {
        EXPECT_EQ(&world, &registry);
        EXPECT_FLOAT_EQ(timeStepSeconds, 0.25f);
        calls.push_back(1);
    });
    registry.addSystem([&](Registry &world, float timeStepSeconds) {
        EXPECT_EQ(&world, &registry);
        EXPECT_FLOAT_EQ(timeStepSeconds, 0.25f);
        calls.push_back(2);
    });
    registry.update(0.25f);
    registry.update(0.25f);
    EXPECT_EQ(calls, (std::vector<int>{1, 2, 1, 2}));
}

TEST(Registry, SystemStatePersistsAcrossUpdates)
{
    Registry registry;
    int observedCount = 0;
    registry.addSystem([count = 0, &observedCount](Registry &, float) mutable {
        count++;
        observedCount = count;
    });
    registry.update(0.f);
    EXPECT_EQ(observedCount, 1);
    registry.update(0.f);
    EXPECT_EQ(observedCount, 2);
}

TEST(Registry, UpdateWithoutSystemsIsSafeAndEmptySystemsAreRejected)
{
    Registry registry;
    EXPECT_NO_THROW(registry.update(0.5f));
    EXPECT_THROW(registry.addSystem(Registry::System{}), std::invalid_argument);
    EXPECT_NO_THROW(registry.update(0.5f));
}
