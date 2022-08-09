/**
 * This file tests class `MapQ`.
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
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
 *
 *       File: map_queue_test.cpp
 * Created On: Jul 21, 2022
 *     Author: Steven R. Emmerson
 */
#include "config.h"
#include "map_queue.h"

#include <climits>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>

namespace {

/// The fixture for testing class `MapQ`
class MapQTest : public ::testing::Test
{
protected:
    struct Frame {
        uint32_t seqNum;
        char     data[4000];
    };
    struct Compare {
        bool operator()(const uint32_t& key1, const uint32_t& key2) {
            return key2 - key1 < UINT32_MAX/2;
        }
    };

    Frame   frames[3];
    Compare comp;

    // You can remove any or all of the following functions if its body
    // is empty.

    MapQTest()
        : frames()
        , comp{}
    {
        for (int i = 0; i < 3; ++i) {
            frames[i].seqNum = i;
            ::memset(frames[i].data, i, 4000);
        }
        // You can do set-up work for each test here.
    }

    virtual ~MapQTest()
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
TEST_F(MapQTest, DefaultConstruction)
{
    MapQ<uint32_t, char[4000], Compare> mq(MapQTest::comp);
}

#if 1

// Tests insertion
TEST_F(MapQTest, Insertion)
{
    MapQ<uint32_t, char[4000], Compare> mq(MapQTest::comp);

    auto pair = mq.insert(frames[1].seqNum, frames[1].data);
    EXPECT_TRUE(pair.second);
    auto equal = frames[1].seqNum == pair.first->first;
    EXPECT_EQ(true, equal);
    EXPECT_EQ(0, ::memcmp(frames[1].data, pair.first->second, 4000);

    const void* key;
    void*       value;

    mq_insert(mq, &key1, &value1);
    EXPECT_EQ(2, mq_size(mq));

    mq_insert(mq, &key3, &value3);
    EXPECT_EQ(3, mq_size(mq));

    EXPECT_EQ(3, mq_size(mq));

    EXPECT_TRUE(mq_front(mq, &key, &value));
    EXPECT_EQ(1, *(unsigned*)key);
    EXPECT_STREQ(value1, (char*)value);
    mq_pop(mq);

    EXPECT_TRUE(mq_front(mq, &key, &value));
    EXPECT_EQ(2, *(unsigned*)key);
    EXPECT_STREQ(value2, (char*)value);
    mq_pop(mq);

    EXPECT_TRUE(mq_front(mq, &key, &value));
    EXPECT_EQ(3, *(unsigned*)key);
    EXPECT_STREQ(value3, (char*)value);
    mq_pop(mq);

    EXPECT_TRUE(mq_empty(mq));

    mq_free(mq);
}
#endif

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
