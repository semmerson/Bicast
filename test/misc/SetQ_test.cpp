/**
 * This file tests class `SetQ`.
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
 *       File: SetQ_test.cpp
 * Created On: Jul 21, 2022
 *     Author: Steven R. Emmerson
 */
#include "config.h"
#include <climits>
#include <gtest/gtest.h>
#include "../../main/SE-III/set_queue.h"

namespace {

/// The fixture for testing class `SetQ`
class SetQTest : public ::testing::Test
{
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    struct Frame {
        uint32_t seqNum;
        char     data[4000];
    };
    struct Compare {
        bool operator()(const Frame& frame1, const Frame& frame2) {
            return frame2.seqNum - frame1.seqNum < UINT32_MAX;
        }
    };

    Frame   frames[3];
    Compare comp;

    SetQTest()
        : frames()
        , comp{}
    {
        for (int i = 0; i < 3; ++i) {
            frames[i].seqNum = i;
            ::memset(frames[i].data, i, 4000);
        }
        // You can do set-up work for each test here.
    }

    virtual ~SetQTest()
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
TEST_F(SetQTest, DefaultConstruction)
{
    SetQ<Frame, Compare> sq(SetQTest::comp);
    EXPECT_EQ(0, sq.size());
    EXPECT_THROW(sq.front(), std::out_of_range);
}

// Tests insertion
TEST_F(SetQTest, Insertion)
{
    SetQ<Frame, Compare> sq(SetQTest::comp);

    sq.insert(frames[1]);
    EXPECT_EQ(1, sq.size());

    sq.insert(frames[0]);
    EXPECT_EQ(2, sq.size());

    sq.insert(frames[2]);
    EXPECT_EQ(3, sq.size());

    for (int i = 0; i < 3; ++i) {
        auto& frame = sq.front();
        //std::cout << "SetQTest::Insertion() Got " << frame << '\n';
        EXPECT_EQ(i, frame.seqNum);
        EXPECT_EQ(0, ::memcmp(frames[i].data, frame.data, 4000));
        sq.pop();
    }

    EXPECT_TRUE(sq.empty());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
