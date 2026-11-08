#include "engine/net/ReliableQueue.hpp"

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

namespace {

using Queue = engine::net::ReliableQueue<std::string>;
using Clock = Queue::Clock;
using std::chrono::milliseconds;

struct Sent {
    std::uint32_t id;
    std::string message;
};

// Runs resendDue() and records what it wanted to send.
std::vector<Sent> due(Queue &queue, Clock::time_point now, std::size_t *retransmissions = nullptr)
{
    std::vector<Sent> sent;
    const auto count =
        queue.resendDue(now, [&sent](std::uint32_t id, const std::string &message) { sent.push_back({id, message}); });
    if (retransmissions)
        *retransmissions = count;
    return sent;
}

const Clock::time_point Start = Clock::time_point{} + std::chrono::seconds(100);

} // namespace

TEST(ReliableQueue, NumbersMessagesFromZeroAndSendsThemRightAway)
{
    Queue queue;
    EXPECT_EQ(queue.push("a"), 0u);
    EXPECT_EQ(queue.push("b"), 1u);

    std::size_t retransmissions = 99;
    const auto sent = due(queue, Start, &retransmissions);
    ASSERT_EQ(sent.size(), 2u);
    EXPECT_EQ(sent[0].id, 0u);
    EXPECT_EQ(sent[1].message, "b");
    EXPECT_EQ(retransmissions, 0u);
    EXPECT_EQ(queue.pending(), 2u);
}

TEST(ReliableQueue, SendsAgainOnlyOnceTheIntervalElapsed)
{
    Queue queue; // 200 ms before any round-trip time is known
    queue.push("a");
    due(queue, Start);

    EXPECT_TRUE(due(queue, Start + milliseconds(199)).empty());
    std::size_t retransmissions = 0;
    const auto sent = due(queue, Start + milliseconds(200), &retransmissions);
    ASSERT_EQ(sent.size(), 1u);
    EXPECT_EQ(retransmissions, 1u);
}

TEST(ReliableQueue, TheWaitDoublesAfterEachUnansweredSend)
{
    Queue queue;
    queue.push("a");
    due(queue, Start);                                           // sent at 0
    EXPECT_EQ(due(queue, Start + milliseconds(200)).size(), 1u); // second send at 200: wait 400 next
    EXPECT_TRUE(due(queue, Start + milliseconds(599)).empty());
    EXPECT_EQ(due(queue, Start + milliseconds(600)).size(), 1u); // third send at 600: wait 800 next
    EXPECT_TRUE(due(queue, Start + milliseconds(1399)).empty());
    EXPECT_EQ(due(queue, Start + milliseconds(1400)).size(), 1u);
}

TEST(ReliableQueue, AcknowledgingStopsTheRepeats)
{
    Queue queue;
    const auto id = queue.push("a");
    due(queue, Start);

    EXPECT_TRUE(queue.acknowledge(id, Start + milliseconds(30)));
    EXPECT_EQ(queue.pending(), 0u);
    EXPECT_TRUE(due(queue, Start + milliseconds(500)).empty());
    EXPECT_FALSE(queue.acknowledge(id, Start + milliseconds(40))) << "a late copy of the acknowledgement";
    EXPECT_FALSE(queue.acknowledge(12345, Start));
}

TEST(ReliableQueue, TheIntervalFollowsTheMeasuredRoundTripTime)
{
    Queue queue;
    const auto id = queue.push("a");
    due(queue, Start);
    EXPECT_EQ(queue.interval(), milliseconds(200)); // nothing measured yet
    queue.acknowledge(id, Start + milliseconds(80));

    ASSERT_TRUE(queue.smoothedRttMs());
    EXPECT_DOUBLE_EQ(*queue.smoothedRttMs(), 80.0);
    EXPECT_EQ(queue.interval(), milliseconds(240)); // 80 + 4 * 40
}

TEST(ReliableQueue, AMessageSentSeveralTimesGivesNoRoundTripSample)
{
    Queue queue;
    const auto id = queue.push("a");
    due(queue, Start);
    due(queue, Start + milliseconds(200)); // sent again: whose copy does the acknowledgement answer?
    queue.acknowledge(id, Start + milliseconds(210));
    EXPECT_FALSE(queue.smoothedRttMs());
}

TEST(ReliableQueue, TheIntervalStaysBetweenItsLimits)
{
    Queue queue;
    auto id = queue.push("fast");
    due(queue, Start);
    queue.acknowledge(id, Start + milliseconds(1));
    EXPECT_EQ(queue.interval(), milliseconds(50)); // never below minInterval

    Queue slow;
    id = slow.push("slow");
    due(slow, Start);
    slow.acknowledge(id, Start + milliseconds(900));
    EXPECT_EQ(slow.interval(), milliseconds(1000)); // never above maxInterval
}

TEST(ReliableQueue, AtMost32NumbersAreInFlightFromTheOldestOne)
{
    Queue queue;
    for (int i = 0; i < 40; ++i)
        queue.push("m" + std::to_string(i));

    const auto first = due(queue, Start);
    ASSERT_EQ(first.size(), 32u);
    EXPECT_EQ(first.back().id, 31u);

    // Number 0 is acknowledged: number 32 now fits in the window.
    queue.acknowledge(0, Start + milliseconds(10));
    const auto next = due(queue, Start + milliseconds(10));
    ASSERT_EQ(next.size(), 1u);
    EXPECT_EQ(next[0].id, 32u);
}

TEST(ReliableQueue, GivesUpOnAMessageThatIsNeverAcknowledged)
{
    Queue::Config config;
    config.maxAttempts = 3;
    Queue queue(config);
    queue.push("lost");

    std::size_t total = 0;
    for (int i = 0; i < 6; ++i)
        total += due(queue, Start + std::chrono::seconds(i * 2)).size();

    EXPECT_EQ(total, 3u);
    EXPECT_EQ(queue.pending(), 0u);
    EXPECT_EQ(queue.abandoned(), 1u);
}
