/**
 * This file tests class `DelayQueue`.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: DelayQueue_test.cpp
 * Created On: Jul 30, 2019
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "DelayQueue.h"

#include "gtest/gtest.h"

namespace {

/// The fixture for testing class `DelayQueue`
class DelayQueueTest : public ::testing::Test
{
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    DelayQueueTest()
    {
        // You can do set-up work for each test here.
    }

    virtual ~DelayQueueTest()
    {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp()
    {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    virtual void TearDown()
    {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    // Objects declared here can be used by all tests in the test case for Error.
};

// Tests default construction
TEST_F(DelayQueueTest, DefaultConstruction)
{
    hycast::DelayQueue<int> dq{};
    EXPECT_TRUE(dq.empty());
}

// Tests immediate reveal
TEST_F(DelayQueueTest, ImmediateReveal)
{
    hycast::DelayQueue<int> dq{};
    const int value = 1;
    dq.push(value, 0);
    EXPECT_FALSE(dq.empty());
    EXPECT_TRUE(dq.ready());
    EXPECT_EQ(value, dq.pop());
}

// Tests delayed reveal
TEST_F(DelayQueueTest, DelayedReveal)
{
    hycast::DelayQueue<int, std::chrono::milliseconds> dq{};
    const int value = 1;
    dq.push(value, 100);
    EXPECT_FALSE(dq.empty());
    EXPECT_FALSE(dq.ready());
    EXPECT_EQ(value, dq.pop());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
