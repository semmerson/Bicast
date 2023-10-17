/**
 * This file tests class `HashMapQueue`.
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
 *       File: HashMapQueue_test.cpp
 * Created On: Jul 12, 2022
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "HashMapQueue.h"

#include <functional>
#include <gtest/gtest.h>

namespace {

using namespace bicast;

/// The fixture for testing class `HashMapQueue`
class HashMapQueueTest : public ::testing::Test
{
protected:
    class Key {
        int val;
    public:
        Key()
            : val(-1)
        {}

        Key(const int val)
            : val(val)
        {}

        operator int() {
            return val;
        }

        size_t hash() const noexcept {
            static auto myHash = std::hash<int>{};
            return myHash(val);
        }

        std::string to_string() const {
            return std::to_string(val);
        }

        bool operator==(const Key& rhs) const {
            return val == rhs.val;
        }
    };

    // You can remove any or all of the following functions if its body
    // is empty.

    HashMapQueueTest()
    {
        // You can do set-up work for each test here.
    }

    virtual ~HashMapQueueTest()
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

// Tests empty queue
TEST_F(HashMapQueueTest, EmptyQueue)
{
    HashMapQueue<Key, std::string> queue{};
    EXPECT_EQ(0, queue.size());
    Key key{1};
    EXPECT_FALSE(queue.erase(key));
}

// Tests one element
TEST_F(HashMapQueueTest, OneElement)
{
    HashMapQueue<Key, std::string> queue{};
    //EXPECT_THROW(queue.front(), OutOfRange); // Calls `terminate()`. Shouldn't
    //EXPECT_THROW(queue.pop(), OutOfRange); // Calls `terminate()`. Shouldn't
    Key key{1};
    queue.pushBack(key, "one");
    EXPECT_EQ(1, queue.size());
    EXPECT_EQ(Key(1), queue.front().first);
    EXPECT_EQ("one", queue.front().second);
    EXPECT_NO_THROW(queue.pop());
    EXPECT_EQ(0, queue.size());
}

// Tests two elements
TEST_F(HashMapQueueTest, TwoElements)
{
    HashMapQueue<Key, std::string> queue{};
    //EXPECT_THROW(queue.front(), OutOfRange); // Calls `terminate()`. Shouldn't
    //EXPECT_THROW(queue.pop(), OutOfRange); // Calls `terminate()`. Shouldn't
    queue.pushBack(1, "one");
    queue.pushBack(2, "two");
    EXPECT_EQ(2, queue.size());

    EXPECT_EQ(Key(1), queue.front().first);
    EXPECT_EQ("one", queue.front().second);
    EXPECT_NO_THROW(queue.pop());
    EXPECT_EQ(1, queue.size());

    EXPECT_EQ(Key(2), queue.front().first);
    EXPECT_EQ("two", queue.front().second);
    EXPECT_NO_THROW(queue.pop());
    EXPECT_EQ(0, queue.size());
}

// Tests erasing head
TEST_F(HashMapQueueTest, EraseHead)
{
    HashMapQueue<Key, std::string> queue{};
    //EXPECT_THROW(queue.front(), OutOfRange); // Calls `terminate()`. Shouldn't
    //EXPECT_THROW(queue.pop(), OutOfRange); // Calls `terminate()`. Shouldn't
    queue.pushBack(1, "one");
    queue.pushBack(2, "two");
    queue.pushBack(3, "three");
    EXPECT_EQ(3, queue.size());

    Key key{1};
    queue.erase(key);
    EXPECT_EQ(2, queue.size());

    EXPECT_EQ(Key(2), queue.front().first);
    EXPECT_EQ("two", queue.front().second);
    EXPECT_NO_THROW(queue.pop());
    EXPECT_EQ(Key(3), queue.front().first);
    EXPECT_EQ("three", queue.front().second);
    EXPECT_NO_THROW(queue.pop());

    EXPECT_EQ(0, queue.size());
}

// Tests erasing interior
TEST_F(HashMapQueueTest, EraseInterior)
{
    HashMapQueue<Key, std::string> queue{};
    //EXPECT_THROW(queue.front(), OutOfRange); // Calls `terminate()`. Shouldn't
    //EXPECT_THROW(queue.pop(), OutOfRange); // Calls `terminate()`. Shouldn't
    queue.pushBack(1, "one");
    queue.pushBack(2, "two");
    queue.pushBack(3, "three");
    EXPECT_EQ(3, queue.size());

    Key key{2};
    queue.erase(key);
    EXPECT_EQ(2, queue.size());

    EXPECT_EQ(Key(1), queue.front().first);
    EXPECT_EQ("one", queue.front().second);
    EXPECT_NO_THROW(queue.pop());
    EXPECT_EQ(Key(3), queue.front().first);
    EXPECT_EQ("three", queue.front().second);
    EXPECT_NO_THROW(queue.pop());

    EXPECT_EQ(0, queue.size());
}

// Tests erasing tail
TEST_F(HashMapQueueTest, EraseTail)
{
    HashMapQueue<Key, std::string> queue{};
    //EXPECT_THROW(queue.front(), OutOfRange); // Calls `terminate()`. Shouldn't
    //EXPECT_THROW(queue.pop(), OutOfRange); // Calls `terminate()`. Shouldn't
    queue.pushBack(1, "one");
    queue.pushBack(2, "two");
    queue.pushBack(3, "three");
    EXPECT_EQ(3, queue.size());

    Key key{3};
    queue.erase(key);
    EXPECT_EQ(2, queue.size());

    EXPECT_EQ(Key(1), queue.front().first);
    EXPECT_EQ("one", queue.front().second);
    EXPECT_NO_THROW(queue.pop());
    EXPECT_EQ(Key(2), queue.front().first);
    EXPECT_EQ("two", queue.front().second);
    EXPECT_NO_THROW(queue.pop());

    EXPECT_EQ(0, queue.size());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
