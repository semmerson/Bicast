/**
 * This file tests class `NoticeQueue`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: NoticeQueue_test.cpp
 * Created On: Dec 6, 2019
 *     Author: Steven R. Emmerson
 */
#include <IdQueue.h>
#include "config.h"

#include "gtest/gtest.h"

namespace {

/// The fixture for testing class `NoticeQueue`
class NoticeQueueTest : public ::testing::Test
{
protected:
    hycast::ProdIndex prodIndex;
    hycast::SegId     segId;

    NoticeQueueTest()
        : prodIndex{1}
        , segId{prodIndex, 1000}
    {}

    virtual ~NoticeQueueTest()
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
TEST_F(NoticeQueueTest, DefaultConstruction)
{
    hycast::IdQueue queue{};
    EXPECT_EQ(0, queue.size());
}

// Tests adding a product-index
TEST_F(NoticeQueueTest, AddProdIndex)
{
    hycast::IdQueue queue{};
    queue.push(prodIndex);
    EXPECT_EQ(1, queue.size());
}

// Tests adding a segment-ID
TEST_F(NoticeQueueTest, AddSegId)
{
    hycast::IdQueue queue{};
    queue.push(segId);
    EXPECT_EQ(1, queue.size());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
