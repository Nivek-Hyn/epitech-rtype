#include "engine/net/LossTracker.hpp"
#include "engine/net/TrafficStats.hpp"

#include <gtest/gtest.h>

using namespace engine::net;

TEST(TrafficCounter, CountsBytesAndPacketsPerDirection)
{
    TrafficCounter counter;
    counter.addSent(100);
    counter.addSent(50);
    counter.addReceived(30);

    const auto totals = counter.totals();
    EXPECT_EQ(totals.bytesSent, 150u);
    EXPECT_EQ(totals.packetsSent, 2u);
    EXPECT_EQ(totals.bytesReceived, 30u);
    EXPECT_EQ(totals.packetsReceived, 1u);
}

TEST(TrafficCounter, ResetClearsEverything)
{
    TrafficCounter counter;
    counter.addSent(10);
    counter.addReceived(10);
    counter.reset();

    const auto totals = counter.totals();
    EXPECT_EQ(totals.bytesSent + totals.bytesReceived + totals.packetsSent + totals.packetsReceived, 0u);
}

TEST(RateMeter, FirstSampleOnlyRecordsAStartingPoint)
{
    RateMeter meter;
    const auto rates = meter.sample(TrafficTotals{1000, 2000, 5, 5}, 10.0);
    EXPECT_DOUBLE_EQ(rates.sentBytesPerSecond, 0.0);
    EXPECT_DOUBLE_EQ(rates.receivedBytesPerSecond, 0.0);
}

TEST(RateMeter, ComputesBytesPerSecondBetweenSamples)
{
    RateMeter meter;
    meter.sample(TrafficTotals{1000, 0, 0, 0}, 10.0);
    const auto rates = meter.sample(TrafficTotals{1000 + 3000, 500, 0, 0}, 12.0);
    EXPECT_DOUBLE_EQ(rates.sentBytesPerSecond, 1500.0);
    EXPECT_DOUBLE_EQ(rates.receivedBytesPerSecond, 250.0);
}

TEST(RateMeter, ZeroElapsedTimeGivesZeroInsteadOfDividingByZero)
{
    RateMeter meter;
    meter.sample(TrafficTotals{0, 0, 0, 0}, 5.0);
    const auto rates = meter.sample(TrafficTotals{100, 100, 1, 1}, 5.0);
    EXPECT_DOUBLE_EQ(rates.sentBytesPerSecond, 0.0);
}

TEST(LossTracker, NoLossWhenEveryNumberArrives)
{
    LossTracker tracker;
    for (std::uint32_t tick = 100; tick < 110; ++tick)
        tracker.record(tick);
    EXPECT_EQ(tracker.expected(), 10u);
    EXPECT_EQ(tracker.received(), 10u);
    EXPECT_DOUBLE_EQ(tracker.lossPercent(), 0.0);
}

TEST(LossTracker, CountsMissingNumbers)
{
    LossTracker tracker;
    for (std::uint32_t tick : {1u, 2u, 3u, 5u, 6u, 9u, 10u})
        tracker.record(tick);
    EXPECT_EQ(tracker.expected(), 10u);
    EXPECT_EQ(tracker.received(), 7u);
    EXPECT_DOUBLE_EQ(tracker.lossPercent(), 30.0);
}

TEST(LossTracker, DuplicatesAndLateNumbersAreNotUseful)
{
    LossTracker tracker;
    for (std::uint32_t tick : {1u, 2u, 2u, 4u, 3u, 5u})
        tracker.record(tick);
    EXPECT_EQ(tracker.expected(), 5u);
    EXPECT_EQ(tracker.received(), 4u); // 1, 2, 4, 5: the duplicate 2 and the late 3 are ignored
    EXPECT_DOUBLE_EQ(tracker.lossPercent(), 20.0);
}

TEST(LossTracker, KeepsWorkingWhenTheCounterWrapsAround)
{
    LossTracker tracker;
    for (std::uint32_t tick : {0xFFFFFFFEu, 0xFFFFFFFFu, 0u, 1u})
        tracker.record(tick);
    EXPECT_EQ(tracker.expected(), 4u);
    EXPECT_DOUBLE_EQ(tracker.lossPercent(), 0.0);
}

TEST(LossTracker, ResetForgetsThePreviousSession)
{
    LossTracker tracker;
    tracker.record(500);
    tracker.record(900);
    tracker.reset();
    EXPECT_EQ(tracker.expected(), 0u);
    EXPECT_DOUBLE_EQ(tracker.lossPercent(), 0.0);
    tracker.record(1);
    EXPECT_DOUBLE_EQ(tracker.lossPercent(), 0.0);
}
