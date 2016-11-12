/**
 * This file tests the class `ChunkInfo`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ChunkInfo_test.cpp
 * @author: Steven R. Emmerson
 */


#include "ChunkInfo.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <sstream>

namespace {

// The fixture for testing class ChunkInfo.
class ChunkInfoTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    ChunkInfoTest() {
        // You can do set-up work for each test here.
    }

    virtual ~ChunkInfoTest() {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp() {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    virtual void TearDown() {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    // Objects declared here can be used by all tests in the test case for
    // ChunkInfo.
};

// Tests construction
TEST_F(ChunkInfoTest, Construction) {
    hycast::ChunkInfo info(1, 2);
    EXPECT_EQ(1, info.getProdIndex());
    EXPECT_EQ(2, info.getChunkIndex());
}

// Tests ChunkInfo::equals()
TEST_F(ChunkInfoTest, Equals) {
    hycast::ChunkInfo info1(1, 2);
    EXPECT_TRUE(info1 == info1);
    hycast::ChunkInfo info2(2, 2);
    EXPECT_FALSE(info1 == info2);
    hycast::ChunkInfo info3(1, 3);
    EXPECT_FALSE(info1 == info3);
}

// Tests serialization/de-serialization
TEST_F(ChunkInfoTest, Serialization) {
    hycast::ChunkInfo info1(1, 2);
    const size_t nbytes = info1.getSerialSize(0);
    alignas(alignof(max_align_t)) char bytes[nbytes];
    info1.serialize(bytes, nbytes, 0);
    hycast::ChunkInfo info2(bytes, nbytes, 0);
    EXPECT_TRUE(info1 == info2);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
