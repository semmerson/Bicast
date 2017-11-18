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


#include "Chunk.h"
#include "ProdInfo.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <sstream>

namespace {

// The fixture for testing class ChunkInfo.
class ChunkInfoTest : public ::testing::Test {
protected:
    ChunkInfoTest()
        : prodSize{4u*hycast::ChunkSize::defaultChunkSize}
    {}

    hycast::ProdSize prodSize;
};

// Tests construction
TEST_F(ChunkInfoTest, Construction) {
    hycast::ProdInfo prodInfo{0, "product", prodSize};
    hycast::ChunkInfo info{prodInfo, 1};
    EXPECT_EQ(0, info.getProdIndex());
    EXPECT_EQ(1, info.getIndex());
}

// Tests operator bool
TEST_F(ChunkInfoTest, OperatorBool) {
    EXPECT_FALSE(hycast::ChunkInfo{});
    hycast::ProdInfo prodInfo{0, "product", prodSize};
    hycast::ChunkInfo chunkInfo{prodInfo, 1};
    EXPECT_TRUE(chunkInfo);
}

// Tests ChunkInfo::equals()
TEST_F(ChunkInfoTest, Equals) {
    hycast::ProdInfo prodInfo{0, "product", prodSize};
    hycast::ChunkInfo info1{prodInfo, 3};
    EXPECT_TRUE(info1 == info1);
    hycast::ProdInfo prodInfo2{1, "product", prodSize};
    hycast::ChunkInfo info2{prodInfo2, 3};
    EXPECT_FALSE(info1 == info2);
    hycast::ProdInfo prodInfo3{0, "product", prodSize-1};
    hycast::ChunkInfo info3{prodInfo3, 3};
    EXPECT_FALSE(info1 == info3);
    hycast::ChunkInfo info4{prodInfo, 2};
    EXPECT_FALSE(info1 == info4);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
