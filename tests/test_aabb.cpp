#include "engine/physics/Aabb.hpp"

#include <gtest/gtest.h>

using engine::physics::Aabb;
using engine::physics::overlaps;

TEST(Aabb, OverlappingBoxesCollide)
{
    const Aabb ship{0.f, 0.f, 10.f, 10.f};
    const Aabb missile{5.f, 5.f, 10.f, 10.f};
    EXPECT_TRUE(overlaps(ship, missile));
    EXPECT_TRUE(overlaps(missile, ship));
}

TEST(Aabb, BoxInsideAnotherCollides)
{
    const Aabb enemy{0.f, 0.f, 100.f, 100.f};
    const Aabb bullet{40.f, 40.f, 2.f, 2.f};
    EXPECT_TRUE(overlaps(enemy, bullet));
    EXPECT_TRUE(overlaps(bullet, enemy));
}

TEST(Aabb, SeparatedBoxesDoNotCollide)
{
    const Aabb ship{0.f, 0.f, 10.f, 10.f};
    EXPECT_FALSE(overlaps(ship, Aabb{20.f, 0.f, 10.f, 10.f}));
    EXPECT_FALSE(overlaps(ship, Aabb{0.f, 20.f, 10.f, 10.f}));
    // Overlapping on x only is not enough: both axes must overlap.
    EXPECT_FALSE(overlaps(ship, Aabb{5.f, 20.f, 10.f, 10.f}));
}

TEST(Aabb, TouchingEdgesDoNotCollide)
{
    const Aabb ship{0.f, 0.f, 10.f, 10.f};
    EXPECT_FALSE(overlaps(ship, Aabb{10.f, 0.f, 10.f, 10.f}));
    EXPECT_FALSE(overlaps(ship, Aabb{0.f, 10.f, 10.f, 10.f}));
}

TEST(Aabb, NegativeCoordinatesWork)
{
    const Aabb offscreen{-10.f, -10.f, 15.f, 15.f};
    EXPECT_TRUE(overlaps(offscreen, Aabb{0.f, 0.f, 10.f, 10.f}));
}
