#include "engine/core/ThreadSafeQueue.hpp"

#include <gtest/gtest.h>

TEST(ThreadSafeQueue, EmptyQueueReturnsNullopt)
{
    engine::core::ThreadSafeQueue<int> queue;
    EXPECT_FALSE(queue.tryPop());
}

TEST(ThreadSafeQueue, PushAndPopPreserveFifoOrder)
{
    engine::core::ThreadSafeQueue<int> queue;
    queue.push(10);
    queue.push(20);

    ASSERT_EQ(queue.tryPop(), 10);
    ASSERT_EQ(queue.tryPop(), 20);
    EXPECT_FALSE(queue.tryPop());
}