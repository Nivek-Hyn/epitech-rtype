#include "engine/core/EventBus.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

using engine::core::EventBus;

namespace {

struct Hit {
    int damage = 0;
};

struct Explosion {
    float radius = 0.f;
};

} // namespace

TEST(EventBus, PublishCallsHandlersWithTheEvent)
{
    EventBus bus;
    int totalDamage = 0;
    bus.subscribe<Hit>([&](const Hit &hit) { totalDamage += hit.damage; });

    bus.publish(Hit{3});
    bus.publish(Hit{4});

    EXPECT_EQ(totalDamage, 7);
}

TEST(EventBus, HandlersRunInSubscriptionOrder)
{
    EventBus bus;
    std::vector<int> calls;
    bus.subscribe<Hit>([&](const Hit &) { calls.push_back(1); });
    bus.subscribe<Hit>([&](const Hit &) { calls.push_back(2); });

    bus.publish(Hit{});

    EXPECT_EQ(calls, (std::vector<int>{1, 2}));
}

TEST(EventBus, HandlersOnlyReceiveTheirEventType)
{
    EventBus bus;
    int hitCount = 0;
    int explosionCount = 0;
    bus.subscribe<Hit>([&](const Hit &) { hitCount++; });
    bus.subscribe<Explosion>([&](const Explosion &) { explosionCount++; });

    bus.publish(Explosion{2.f});

    EXPECT_EQ(hitCount, 0);
    EXPECT_EQ(explosionCount, 1);
}

TEST(EventBus, PublishingWithoutSubscribersIsSafe)
{
    const EventBus bus;
    EXPECT_NO_THROW(bus.publish(Hit{1}));
}

TEST(EventBus, EmptyHandlersAreRejected)
{
    EventBus bus;
    EXPECT_THROW(bus.subscribe<Hit>(nullptr), std::invalid_argument);
}
