#include "engine/ecs/Registry.hpp"

#include <gtest/gtest.h>

using engine::ecs::Entity;
using engine::ecs::Registry;

namespace {
struct Pos {
    float x = 0.f;
    float y = 0.f;
};
struct Vel {
    float x = 0.f;
    float y = 0.f;
};
} // namespace

TEST(Registry, SpawnGivesDistinctIds)
{
    Registry reg;
    EXPECT_NE(reg.spawn(), reg.spawn());
}

TEST(Registry, KillRemovesComponentsAndReusesId)
{
    Registry reg;
    const Entity e = reg.spawn();
    reg.emplace<Pos>(e, 1.f, 2.f);
    reg.kill(e);
    EXPECT_FALSE(reg.alive(e));
    EXPECT_FALSE(reg.has<Pos>(e));
    EXPECT_EQ(reg.spawn(), e);
}

TEST(Registry, EachVisitsOnlyEntitiesWithAllComponents)
{
    Registry reg;
    const Entity both = reg.spawn();
    const Entity onlyPos = reg.spawn();
    reg.emplace<Pos>(both);
    reg.emplace<Vel>(both, 1.f, 0.f);
    reg.emplace<Pos>(onlyPos);

    int visited = 0;
    reg.each<Pos, Vel>([&](Entity e, Pos &, Vel &) {
        EXPECT_EQ(e, both);
        ++visited;
    });
    EXPECT_EQ(visited, 1);
}

TEST(Registry, SystemsRunInOrder)
{
    Registry reg;
    const Entity e = reg.spawn();
    reg.emplace<Pos>(e);
    reg.emplace<Vel>(e, 10.f, 0.f);
    reg.addSystem([](Registry &r, float dt) { r.each<Pos, Vel>([dt](Entity, Pos &p, Vel &v) { p.x += v.x * dt; }); });
    reg.update(0.5f);
    EXPECT_FLOAT_EQ(reg.get<Pos>(e)->x, 5.f);
}
