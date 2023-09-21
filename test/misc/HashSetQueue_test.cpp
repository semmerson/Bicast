/**
 * This file tests class `HashSetQueue`.
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
 *       File: HashSetQueue_test.cpp
 * Created On: Jul 12, 2022
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "HashSetQueue.h"

#include <functional>
#include <gtest/gtest.h>

namespace {

using namespace hycast;

/// The fixture for testing class `HashSetQueue`
class HashSetQueueTest : public testing::Test
{
protected:
    class Elt {
        int val;
    public:
        Elt()
            : val(-1)
        {}

        Elt(const int val)
            : val(val)
        {}

        operator int() {
            return val;
        }

        size_t hash() const noexcept {
            static auto myHash = std::hash<int>{};
            return myHash(val);
        }

        std::string to_string() {
            return std::to_string(val);
        }

        bool operator==(const Elt& rhs) const {
            return val == rhs.val;
        }
    };

    // You can remove any or all of the following functions if its body
    // is empty.

    HashSetQueueTest()
    {
        // You can do set-up work for each test here.
    }

    virtual ~HashSetQueueTest()
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
TEST_F(HashSetQueueTest, EmptyQueue)
{
    HashSetQueue<Elt> queue{};
    EXPECT_EQ(0, queue.size());
    EXPECT_FALSE(queue.erase(1));
}

// Tests one element
TEST_F(HashSetQueueTest, OneElement)
{
    HashSetQueue<Elt> queue{};
    //EXPECT_THROW(queue.front(), OutOfRange); // Calls `terminate()`. Shouldn't
    //EXPECT_THROW(queue.pop(), OutOfRange); // Calls `terminate()`. Shouldn't
    queue.push(1);
    EXPECT_EQ(1, queue.size());
    EXPECT_EQ(Elt(1), queue.front());
    EXPECT_NO_THROW(queue.pop());
    EXPECT_EQ(0, queue.size());
}

// Tests two elements
TEST_F(HashSetQueueTest, TwoElements)
{
    HashSetQueue<Elt> queue{};
    //EXPECT_THROW(queue.front(), OutOfRange); // Calls `terminate()`. Shouldn't
    //EXPECT_THROW(queue.pop(), OutOfRange); // Calls `terminate()`. Shouldn't
    queue.push(1);
    queue.push(2);
    EXPECT_EQ(2, queue.size());

    EXPECT_EQ(Elt(1), queue.front());
    EXPECT_NO_THROW(queue.pop());
    EXPECT_EQ(1, queue.size());

    EXPECT_EQ(Elt(2), queue.front());
    EXPECT_NO_THROW(queue.pop());
    EXPECT_EQ(0, queue.size());
}

// Tests erasing head
TEST_F(HashSetQueueTest, EraseHead)
{
    HashSetQueue<Elt> queue{};
    //EXPECT_THROW(queue.front(), OutOfRange); // Calls `terminate()`. Shouldn't
    //EXPECT_THROW(queue.pop(), OutOfRange); // Calls `terminate()`. Shouldn't
    queue.push(1);
    queue.push(2);
    queue.push(3);
    EXPECT_EQ(3, queue.size());

    queue.erase(1);
    EXPECT_EQ(2, queue.size());

    EXPECT_EQ(Elt(2), queue.front());
    EXPECT_NO_THROW(queue.pop());
    EXPECT_EQ(Elt(3), queue.front());
    EXPECT_NO_THROW(queue.pop());

    EXPECT_EQ(0, queue.size());
}

// Tests erasing interior
TEST_F(HashSetQueueTest, EraseInterior)
{
    HashSetQueue<Elt> queue{};
    //EXPECT_THROW(queue.front(), OutOfRange); // Calls `terminate()`. Shouldn't
    //EXPECT_THROW(queue.pop(), OutOfRange); // Calls `terminate()`. Shouldn't
    queue.push(1);
    queue.push(2);
    queue.push(3);
    EXPECT_EQ(3, queue.size());

    queue.erase(2);
    EXPECT_EQ(2, queue.size());

    EXPECT_EQ(Elt(1), queue.front());
    EXPECT_NO_THROW(queue.pop());
    EXPECT_EQ(Elt(3), queue.front());
    EXPECT_NO_THROW(queue.pop());

    EXPECT_EQ(0, queue.size());
}

// Tests erasing tail
TEST_F(HashSetQueueTest, EraseTail)
{
    HashSetQueue<Elt> queue{};
    //EXPECT_THROW(queue.front(), OutOfRange); // Calls `terminate()`. Shouldn't
    //EXPECT_THROW(queue.pop(), OutOfRange); // Calls `terminate()`. Shouldn't
    queue.push(1);
    queue.push(2);
    queue.push(3);
    EXPECT_EQ(3, queue.size());

    queue.erase(3);
    EXPECT_EQ(2, queue.size());

    EXPECT_EQ(Elt(1), queue.front());
    EXPECT_NO_THROW(queue.pop());
    EXPECT_EQ(Elt(2), queue.front());
    EXPECT_NO_THROW(queue.pop());

    EXPECT_EQ(0, queue.size());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
