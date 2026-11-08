#include "engine/net/SequenceWindow.hpp"

#include <gtest/gtest.h>

using engine::net::SequenceVerdict;
using engine::net::SequenceWindow;

TEST(SequenceWindow, PacketsInOrderAreAllNewest)
{
    SequenceWindow window;
    for (std::uint32_t sequence = 0; sequence < 100; ++sequence)
        EXPECT_EQ(window.receive(sequence), SequenceVerdict::Newest);
}

TEST(SequenceWindow, AGapIsNotAProblemByItself)
{
    SequenceWindow window;
    EXPECT_EQ(window.receive(1), SequenceVerdict::Newest);
    EXPECT_EQ(window.receive(5), SequenceVerdict::Newest); // 2, 3 and 4 were lost or are late
}

TEST(SequenceWindow, TheSameNumberTwiceIsADuplicate)
{
    SequenceWindow window;
    window.receive(7);
    EXPECT_EQ(window.receive(7), SequenceVerdict::Duplicate);
    window.receive(8);
    EXPECT_EQ(window.receive(7), SequenceVerdict::Duplicate);
    EXPECT_EQ(window.receive(8), SequenceVerdict::Duplicate);
}

TEST(SequenceWindow, ALowerNumberNeverSeenIsLateOnlyOnce)
{
    SequenceWindow window;
    window.receive(10);
    window.receive(14);
    EXPECT_EQ(window.receive(12), SequenceVerdict::Late);
    EXPECT_EQ(window.receive(12), SequenceVerdict::Duplicate);
    EXPECT_EQ(window.receive(11), SequenceVerdict::Late);
}

TEST(SequenceWindow, TheWindowCoversTheLast32Numbers)
{
    SequenceWindow window;
    window.receive(100);
    EXPECT_EQ(window.receive(69), SequenceVerdict::Late);   // 31 behind: inside
    EXPECT_EQ(window.receive(68), SequenceVerdict::TooOld); // 32 behind: outside
    EXPECT_EQ(window.receive(1), SequenceVerdict::TooOld);
}

TEST(SequenceWindow, ForgetsNumbersThatSlidOutOfTheWindow)
{
    SequenceWindow window;
    window.receive(0);
    window.receive(40); // 0 is now 40 behind
    EXPECT_EQ(window.receive(0), SequenceVerdict::TooOld);
    window.receive(200); // a jump larger than the window clears the memory
    EXPECT_EQ(window.receive(199), SequenceVerdict::Late);
    EXPECT_EQ(window.receive(40), SequenceVerdict::TooOld);
}

TEST(SequenceWindow, KeepsWorkingWhenTheCounterWrapsAround)
{
    SequenceWindow window;
    EXPECT_EQ(window.receive(0xFFFFFFFEu), SequenceVerdict::Newest);
    EXPECT_EQ(window.receive(0xFFFFFFFFu), SequenceVerdict::Newest);
    EXPECT_EQ(window.receive(0u), SequenceVerdict::Newest);
    EXPECT_EQ(window.receive(1u), SequenceVerdict::Newest);
    EXPECT_EQ(window.receive(0xFFFFFFFFu), SequenceVerdict::Duplicate);
    EXPECT_EQ(window.receive(0xFFFFFFFDu), SequenceVerdict::Late);
}

TEST(SequenceWindow, ResetStartsFromScratch)
{
    SequenceWindow window;
    window.receive(5000);
    window.reset();
    EXPECT_FALSE(window.started());
    EXPECT_EQ(window.receive(3), SequenceVerdict::Newest);
    EXPECT_EQ(window.highest(), 3u);
}
