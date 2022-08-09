/**
 * This file tests class `MapQueue`.
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

#include <gtest/gtest.h>

namespace {

/// The fixture for testing class `MapQueue`
class MapQueueTest : public ::testing::Test
{
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    MapQueueTest()
    {
        // You can do set-up work for each test here.
    }

    virtual ~MapQueueTest()
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

    static int compare(const void* key1, const void* key2) {
        unsigned uint1 = *(unsigned*)key1;
        unsigned uint2 = *(unsigned*)key2;
        return uint1 - uint2;
    }
};

// Tests default construction
TEST_F(MapQueueTest, DefaultConstruction)
{
    MapQueue* mq = sq_alloc(&MapQueueTest::compare);
    EXPECT_EQ(0, mq_size(mq));
    EXPECT_TRUE(mq_empty(mq));
    const void* key;
    void*       value;
    EXPECT_FALSE(mq_front(mq, &key, &value));
    mq_free(mq);
}

// Tests insertion
TEST_F(MapQueueTest, Insertion)
{
    unsigned  key1 = 1;
    unsigned  key2 = 2;
    unsigned  key3 = 3;
    char      value1[] = "1";
    char      value2[] = "2";
    char      value3[] = "3";
    MapQueue* mq = sq_alloc(&MapQueueTest::compare);

    mq_insert(mq, &key2, &value2);
    EXPECT_EQ(1, mq_size(mq));

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

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
