#include "engine/ecs/Prefabs.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

struct Health {
    int hp = 0;
};
struct Speed {
    float value = 0.f;
};

// The engine does not know the data format: here, a plain string holding a number.
engine::ecs::Prefabs<std::string> makePrefabs()
{
    engine::ecs::Prefabs<std::string> prefabs;
    prefabs.registerComponent<Health>("Health", [](const std::string &data) { return Health{std::stoi(data)}; });
    prefabs.registerComponent<Speed>("Speed", [](const std::string &data) { return Speed{std::stof(data)}; });
    prefabs.add("grunt", {{"Health", "3"}, {"Speed", "120"}});
    return prefabs;
}

} // namespace

TEST(Prefabs, SpawnCreatesAnEntityWithEveryComponent)
{
    engine::ecs::Registry registry;
    const auto prefabs = makePrefabs();
    const engine::ecs::Entity grunt = prefabs.spawn(registry, "grunt");

    ASSERT_NE(registry.get<Health>(grunt), nullptr);
    EXPECT_EQ(registry.get<Health>(grunt)->hp, 3);
    EXPECT_FLOAT_EQ(registry.get<Speed>(grunt)->value, 120.f);
}

TEST(Prefabs, ApplyToReplacesTheComponentsOfAnExistingEntity)
{
    engine::ecs::Registry registry;
    const auto prefabs = makePrefabs();
    const engine::ecs::Entity entity = registry.spawn();
    registry.emplace<Health>(entity, 99);

    prefabs.applyTo(registry, entity, "grunt");

    EXPECT_EQ(registry.get<Health>(entity)->hp, 3);
}

TEST(Prefabs, UnknownNamesAreRejected)
{
    engine::ecs::Registry registry;
    auto prefabs = makePrefabs();

    EXPECT_THROW(prefabs.add("broken", {{"Shield", "1"}}), std::invalid_argument);
    EXPECT_FALSE(prefabs.contains("broken"));
    EXPECT_THROW(prefabs.spawn(registry, "missing"), std::out_of_range);
    EXPECT_FALSE(registry.alive(0)); // nothing was created
}
