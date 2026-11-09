#include "common/SnapshotInterpolator.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace {

using namespace rtype::protocol;
using Clock = SnapshotInterpolator::Clock;
using std::chrono::microseconds;

// Server tick k happens at k/60 s; a time of `tick` ticks after the origin.
const Clock::time_point Origin = Clock::time_point{} + std::chrono::seconds(1000);

Clock::time_point atTick(double tick)
{
    return Origin + microseconds(static_cast<long long>(tick * 1e6 / 60.0));
}

struct Entity {
    std::uint32_t id;
    float x;
    float y;
};

Snapshot snapshotOf(std::uint32_t tick, std::vector<Entity> entities)
{
    Snapshot snapshot;
    snapshot.tick = tick;
    for (const Entity &entity : entities)
        snapshot.entities.push_back({entity.id, EntityType::Enemy, 0, entity.x, entity.y});
    return snapshot;
}

const EntityState *find(const std::vector<EntityState> &entities, std::uint32_t id)
{
    for (const EntityState &entity : entities) {
        if (entity.netId == id)
            return &entity;
    }
    return nullptr;
}

} // namespace

TEST(SnapshotInterpolator, TheFirstSnapshotIsShownAtOnce)
{
    SnapshotInterpolator interpolator;
    EXPECT_TRUE(interpolator.sample(atTick(0)).empty());
    interpolator.push(snapshotOf(2, {{1, 10.f, 0.f}}), atTick(2));

    const auto released = interpolator.popDue(atTick(2));
    ASSERT_TRUE(released);
    EXPECT_EQ(released->tick, 2u);
    EXPECT_FALSE(interpolator.popDue(atTick(2)));
    EXPECT_EQ(interpolator.sample(atTick(2)).size(), 1u);
}

TEST(SnapshotInterpolator, SnapshotsAreReleasedInOrderWhenTheirTimeComes)
{
    SnapshotInterpolator interpolator; // 2 intervals of 2 ticks: shown 4 ticks late
    interpolator.push(snapshotOf(2, {{1, 0.f, 0.f}}), atTick(2));
    interpolator.push(snapshotOf(4, {{1, 10.f, 0.f}}), atTick(4));
    interpolator.push(snapshotOf(6, {{1, 20.f, 0.f}}), atTick(6));
    EXPECT_DOUBLE_EQ(interpolator.delayTicks(), 4.0);

    ASSERT_TRUE(interpolator.popDue(atTick(6))); // tick 2: the first one is released at once
    EXPECT_FALSE(interpolator.popDue(atTick(7.9))) << "tick 4 is shown at 4 + 4 = 8";
    const auto second = interpolator.popDue(atTick(8.1));
    ASSERT_TRUE(second);
    EXPECT_EQ(second->tick, 4u);
    EXPECT_FALSE(interpolator.popDue(atTick(9.9)));
    const auto third = interpolator.popDue(atTick(10.1));
    ASSERT_TRUE(third);
    EXPECT_EQ(third->tick, 6u);
}

TEST(SnapshotInterpolator, AnEntityMovesInAStraightLineBetweenTwoSnapshots)
{
    SnapshotInterpolator interpolator;
    interpolator.push(snapshotOf(2, {{1, 0.f, 100.f}}), atTick(2));
    interpolator.push(snapshotOf(4, {{1, 10.f, 140.f}}), atTick(4));
    interpolator.popDue(atTick(6)); // tick 2 is shown

    // At time 7 the world shown is tick 3: halfway between ticks 2 and 4.
    const auto halfway = interpolator.sample(atTick(7));
    ASSERT_EQ(halfway.size(), 1u);
    EXPECT_NEAR(halfway[0].x, 5.f, 0.01f);
    EXPECT_NEAR(halfway[0].y, 120.f, 0.05f);

    const auto quarter = interpolator.sample(atTick(6.5));
    EXPECT_NEAR(quarter[0].x, 2.5f, 0.01f);
}

TEST(SnapshotInterpolator, TheLastSnapshotIsHeldWhenNothingNewArrives)
{
    SnapshotInterpolator interpolator;
    interpolator.push(snapshotOf(2, {{1, 5.f, 5.f}}), atTick(2));
    interpolator.popDue(atTick(2));

    const auto shown = interpolator.sample(atTick(60)); // a second later, still nothing
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_FLOAT_EQ(shown[0].x, 5.f);
}

TEST(SnapshotInterpolator, ALostSnapshotIsHiddenByItsNeighbours)
{
    SnapshotInterpolator interpolator;
    interpolator.push(snapshotOf(2, {{1, 0.f, 0.f}}), atTick(2));
    interpolator.push(snapshotOf(4, {{1, 20.f, 0.f}}), atTick(4));
    interpolator.push(snapshotOf(8, {{1, 60.f, 0.f}}), atTick(8)); // tick 6 never arrived
    EXPECT_DOUBLE_EQ(interpolator.intervalTicks(), 2.0) << "a gap is a lost snapshot, not a slower server";

    ASSERT_TRUE(interpolator.popDue(atTick(7))) << "tick 2";
    EXPECT_FALSE(interpolator.popDue(atTick(7.5))) << "tick 4 is shown at 8";
    ASSERT_TRUE(interpolator.popDue(atTick(8.1))) << "tick 4";
    // Shown time 6 is halfway between ticks 4 (x = 20) and 8 (x = 60).
    const auto shown = interpolator.sample(atTick(10));
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_NEAR(shown[0].x, 40.f, 0.05f);
}

TEST(SnapshotInterpolator, ANewEntityAppearsWithItsSnapshotAndARemovedOneStaysUntilThen)
{
    SnapshotInterpolator interpolator;
    interpolator.push(snapshotOf(2, {{1, 0.f, 0.f}, {2, 50.f, 0.f}}), atTick(2));
    interpolator.push(snapshotOf(4, {{1, 10.f, 0.f}, {3, 99.f, 0.f}}), atTick(4));
    interpolator.popDue(atTick(6)); // tick 2 is shown

    const auto between = interpolator.sample(atTick(7));
    EXPECT_TRUE(find(between, 1));
    EXPECT_TRUE(find(between, 2)) << "destroyed in the next snapshot: still shown until that snapshot is released";
    EXPECT_FLOAT_EQ(find(between, 2)->x, 50.f) << "and it does not move";
    EXPECT_FALSE(find(between, 3)) << "born in the next snapshot: not shown yet";

    ASSERT_TRUE(interpolator.popDue(atTick(8)));
    const auto after = interpolator.sample(atTick(8));
    EXPECT_FALSE(find(after, 2));
    EXPECT_TRUE(find(after, 3));
}

TEST(SnapshotInterpolator, TheIntervalOfTheServerIsMeasured)
{
    SnapshotInterpolator interpolator;
    interpolator.push(snapshotOf(3, {}), atTick(3));
    EXPECT_DOUBLE_EQ(interpolator.delayTicks(), 4.0) << "a guess of 2 ticks until two snapshots were seen";
    interpolator.push(snapshotOf(6, {}), atTick(6));
    interpolator.push(snapshotOf(9, {}), atTick(9));
    EXPECT_DOUBLE_EQ(interpolator.intervalTicks(), 3.0);
    EXPECT_DOUBLE_EQ(interpolator.delayTicks(), 6.0);
}

TEST(SnapshotInterpolator, OldAndRepeatedSnapshotsAreIgnoredButACompleterOneReplaces)
{
    SnapshotInterpolator interpolator;
    interpolator.push(snapshotOf(4, {{1, 1.f, 0.f}}), atTick(4));
    interpolator.push(snapshotOf(2, {{1, 99.f, 0.f}}), atTick(4.5));               // older than the newest known
    interpolator.push(snapshotOf(4, {{1, 1.f, 0.f}, {2, 2.f, 0.f}}), atTick(4.6)); // more complete: replaces

    const auto released = interpolator.popDue(atTick(5));
    ASSERT_TRUE(released);
    EXPECT_EQ(released->tick, 4u);
    EXPECT_EQ(released->entities.size(), 2u);

    interpolator.push(snapshotOf(3, {{1, 7.f, 0.f}}), atTick(6)); // older than the one being shown
    EXPECT_FALSE(interpolator.popDue(atTick(60)));
}

TEST(SnapshotInterpolator, ResetStartsAgainForANewSession)
{
    SnapshotInterpolator interpolator;
    interpolator.push(snapshotOf(500, {{1, 1.f, 0.f}}), atTick(500));
    interpolator.popDue(atTick(500));
    interpolator.reset();
    EXPECT_TRUE(interpolator.sample(atTick(501)).empty());

    interpolator.push(snapshotOf(2, {{9, 4.f, 0.f}}), atTick(502)); // ticks start again from zero
    ASSERT_TRUE(interpolator.popDue(atTick(502)));
    EXPECT_TRUE(find(interpolator.sample(atTick(502)), 9));
}

// An entity flies at 400 units per second (6.67 per tick). The server sends a snapshot every 2 ticks and the network
// delays each one by 40 ms plus a random 0 to 30 ms. The screen is drawn 60 times a second.
TEST(SnapshotInterpolator, MovementIsSmootherThanDrawingEachSnapshotWhenItArrives)
{
    struct Arrival {
        double time;
        std::uint32_t tick;
    };
    std::mt19937 random(7);
    std::uniform_real_distribution<double> jitter(0.0, 0.030);
    std::vector<Arrival> arrivals;
    for (std::uint32_t tick = 2; tick < 400; tick += 2)
        arrivals.push_back({static_cast<double>(tick) / 60.0 + 0.040 + jitter(random), tick});
    std::stable_sort(arrivals.begin(), arrivals.end(),
                     [](const Arrival &a, const Arrival &b) { return a.time < b.time; });
    const auto positionAt = [](std::uint32_t tick) { return 400.f * static_cast<float>(tick) / 60.f; };

    SnapshotInterpolator interpolator;
    std::size_t next = 0;
    std::uint32_t newestTick = 0;
    float previousRaw = 0.f;
    float previousSmooth = 0.f;
    float largestRawStep = 0.f;
    float largestSmoothStep = 0.f;

    // From the start, one frame at a time, so that each snapshot is stamped with the frame in which it is received.
    // The first 0.6 s let the clock estimate settle; the steps are measured after that.
    for (double time = 0.0; time < 6.0; time += 1.0 / 60.0) {
        const auto now = Origin + microseconds(static_cast<long long>(time * 1e6));
        while (next < arrivals.size() && arrivals[next].time <= time) {
            interpolator.push(snapshotOf(arrivals[next].tick, {{1, positionAt(arrivals[next].tick), 0.f}}), now);
            newestTick = std::max(newestTick, arrivals[next].tick);
            ++next;
        }
        while (interpolator.popDue(now)) {
        }
        const auto smooth = interpolator.sample(now);
        if (smooth.empty())
            continue;                             // nothing has arrived yet
        const float raw = positionAt(newestTick); // what drawing each snapshot on arrival shows
        if (time > 0.6) {
            largestRawStep = std::max(largestRawStep, raw - previousRaw);
            largestSmoothStep = std::max(largestSmoothStep, smooth[0].x - previousSmooth);
            EXPECT_GE(smooth[0].x, previousSmooth) << "an entity must never move backwards";
        }
        previousRaw = raw;
        previousSmooth = smooth[0].x;
    }

    const float nominalStep = 400.f / 60.f;
    EXPECT_LT(largestSmoothStep, largestRawStep);
    EXPECT_LT(largestSmoothStep, 1.5f * nominalStep) << "the interpolated entity advances at about its real speed";
    EXPECT_GT(largestRawStep, 1.9f * nominalStep) << "without interpolation the entity jumps by two steps at once";
}
