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
#include <ChunkIdQueue.h>
#include "config.h"

#include "gtest/gtest.h"

namespace {

/// The fixture for testing class `NoticeQueue`
class NoticeQueueTest : public ::testing::Test
{
protected:
    hycast::ProdIndex prodId;
    hycast::SegId     segId;

    NoticeQueueTest()
        : prodId{1}
        , segId{prodId, 1000}
    {}
};

// Tests default construction
TEST_F(NoticeQueueTest, DefaultConstruction)
{
    hycast::ChunkIdQueue queue{};
    EXPECT_EQ(0, queue.size());
}

// Tests adding a product-index
TEST_F(NoticeQueueTest, AddProdIndex)
{
    hycast::ChunkIdQueue queue{};
    hycast::ChunkId chunkId{prodId};
    queue.push(chunkId);
    EXPECT_EQ(1, queue.size());
}

// Tests adding a segment-ID
TEST_F(NoticeQueueTest, AddSegId)
{
    hycast::ChunkIdQueue queue{};
    hycast::ChunkId chunkId{segId};
    queue.push(chunkId);
    EXPECT_EQ(1, queue.size());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
