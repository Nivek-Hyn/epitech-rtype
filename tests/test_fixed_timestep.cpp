#include "engine/core/FixedTimestep.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <thread>

using engine::core::FixedTimestep;

TEST(FixedTimestep, StepIsTheInverseOfTheTickRate)
{
    FixedTimestep clock(60.0);
    EXPECT_FLOAT_EQ(clock.step(), 1.f / 60.f);
    FixedTimestep slowerClock(20.0);
    EXPECT_FLOAT_EQ(slowerClock.step(), 0.05f);
}

TEST(FixedTimestep, RejectsInvalidRatesAndCatchUpLimits)
{
    EXPECT_THROW(FixedTimestep{0.0}, std::invalid_argument);
    EXPECT_THROW(FixedTimestep{-60.0}, std::invalid_argument);
    EXPECT_THROW(FixedTimestep{std::numeric_limits<double>::infinity()}, std::invalid_argument);
    EXPECT_THROW(FixedTimestep{std::numeric_limits<double>::quiet_NaN()}, std::invalid_argument);
    EXPECT_THROW((FixedTimestep{60.0, 0}), std::invalid_argument);
    EXPECT_THROW((FixedTimestep{60.0, -1}), std::invalid_argument);
    EXPECT_THROW(FixedTimestep{std::numeric_limits<double>::max()}, std::invalid_argument);
    EXPECT_THROW(FixedTimestep{std::numeric_limits<double>::min()}, std::invalid_argument);
}

TEST(FixedTimestep, WaitingDurationIsNonnegativeAndAtMostOneStep)
{
    FixedTimestep clock(60.0);
    const auto remaining = clock.untilNextTick();
    EXPECT_GE(remaining, FixedTimestep::Clock::duration::zero());
    EXPECT_LE(std::chrono::duration<double>(remaining).count(), 1.0 / 60.0);
}

TEST(FixedTimestep, ElapsedTimeMakesATickDueWithoutCallingAdvance)
{
    FixedTimestep clock(1000.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(clock.untilNextTick(), FixedTimestep::Clock::duration::zero());
    EXPECT_EQ(clock.advance(), 5);
}

TEST(FixedTimestep, CatchUpHonorsTheConfiguredLimit)
{
    FixedTimestep clock(1000.0, 2);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(clock.advance(), 2);
    EXPECT_FLOAT_EQ(clock.step(), 0.001f);
}

TEST(FixedTimestep, SuccessiveAdvancesPreserveElapsedTimeBetweenCalls)
{
    // Bound the real elapsed time instead of assuming sleep_for wakes up exactly on time.
    const auto beforeConstruction = FixedTimestep::Clock::now();
    FixedTimestep clock(100.0, 1000000);
    const auto afterConstruction = FixedTimestep::Clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    const int firstTicks = clock.advance();
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    const auto beforeAdvance = FixedTimestep::Clock::now();
    const int totalTicks = firstTicks + clock.advance();
    const auto afterAdvance = FixedTimestep::Clock::now();

    const double minElapsedSeconds = std::chrono::duration<double>(beforeAdvance - afterConstruction).count();
    const double maxElapsedSeconds = std::chrono::duration<double>(afterAdvance - beforeConstruction).count();
    EXPECT_GE(static_cast<double>(totalTicks), std::floor(minElapsedSeconds / 0.01));
    EXPECT_LE(static_cast<double>(totalTicks), std::floor(maxElapsedSeconds / 0.01));
}
