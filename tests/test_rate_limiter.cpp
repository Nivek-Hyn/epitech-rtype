#include "engine/net/RateLimiter.hpp"

#include <gtest/gtest.h>

using engine::net::RateLimiter;
using Clock = RateLimiter::Clock;
using std::chrono::milliseconds;

namespace {

const Clock::time_point Start = Clock::time_point{} + std::chrono::seconds(500);

int allowedAtOnce(RateLimiter &limiter, Clock::time_point now, int attempts)
{
    int allowed = 0;
    for (int i = 0; i < attempts; ++i)
        allowed += limiter.allow(now);
    return allowed;
}

} // namespace

TEST(RateLimiter, AllowsABurstThenRefuses)
{
    RateLimiter limiter(100.0, 10.0);
    EXPECT_EQ(allowedAtOnce(limiter, Start, 50), 10);
}

TEST(RateLimiter, TokensComeBackAtTheSteadyRate)
{
    RateLimiter limiter(100.0, 10.0); // 100 per second = 1 every 10 ms
    allowedAtOnce(limiter, Start, 10);
    EXPECT_FALSE(limiter.allow(Start + milliseconds(5)));
    EXPECT_EQ(allowedAtOnce(limiter, Start + milliseconds(100), 50), 10) << "a tenth of a second gives 10 tokens";
}

TEST(RateLimiter, NeverStoresMoreThanTheBurst)
{
    RateLimiter limiter(100.0, 10.0);
    allowedAtOnce(limiter, Start, 10);
    EXPECT_EQ(allowedAtOnce(limiter, Start + std::chrono::seconds(60), 50), 10) << "a long silence is not saved up";
}

TEST(RateLimiter, ALongRunOfFloodingLetsThroughOnlyTheSteadyRate)
{
    RateLimiter limiter(100.0, 20.0);
    int allowed = 0;
    for (int millisecond = 0; millisecond < 10000; ++millisecond)          // 10 seconds
        for (int packet = 0; packet < 20; ++packet)                         // 20000 packets per second
            allowed += limiter.allow(Start + milliseconds(millisecond));
    EXPECT_NEAR(allowed, 100 * 10 + 20, 5);
}

TEST(RateLimiter, APeerBelowTheRateIsNeverRefused)
{
    RateLimiter limiter(100.0, 5.0);
    for (int packet = 0; packet < 1000; ++packet)
        EXPECT_TRUE(limiter.allow(Start + milliseconds(packet * 16))) << "60 packets per second against 100 allowed";
}

TEST(RateLimiter, ATimeThatGoesBackwardsDoesNotGiveTokens)
{
    RateLimiter limiter(100.0, 3.0);
    allowedAtOnce(limiter, Start + milliseconds(1000), 3);
    EXPECT_FALSE(limiter.allow(Start));
}
