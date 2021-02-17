/**
 * This file tests class `NoticeQueue`.
 *
 *       File: NoticeQueue_test.cpp
 * Created On: Dec 6, 2019
 *     Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"

#include "ChunkIdQueue.h"

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
    EXPECT_EQ(chunkId, queue.pop());
}

// Tests adding a segment-ID
TEST_F(NoticeQueueTest, AddSegId)
{
    hycast::ChunkIdQueue queue{};
    hycast::ChunkId chunkId{segId};
    queue.push(chunkId);
    EXPECT_EQ(1, queue.size());
    EXPECT_EQ(chunkId, queue.pop());
}

#if 0
// Tests begin() and end()
TEST_F(NoticeQueueTest, BeginAndEnd)
{
    hycast::ChunkIdQueue queue{};
    hycast::ChunkId chunkId{segId};
    queue.push(chunkId);
    EXPECT_EQ(1, queue.size());
}
#endif

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
