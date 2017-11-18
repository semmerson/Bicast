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


#include <ChunkId.h>
#include "ProdInfo.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <sstream>

namespace {

// The fixture for testing class ChunkInfo.
class ChunkInfoTest : public ::testing::Test {
protected:
    ChunkInfoTest()
        : prodSize{4u*hycast::ChunkId::getCanonSize()}
    {}

    hycast::ProdSize prodSize;
};

// Tests construction
TEST_F(ChunkInfoTest, Construction) {
    hycast::ProdInfo prodInfo("product", 0, prodSize);
    hycast::ChunkId info(prodInfo, 1);
    EXPECT_EQ(0, info.getProdIndex());
    EXPECT_EQ(1, info.getIndex());
}

// Tests operator bool
TEST_F(ChunkInfoTest, OperatorBool) {
    EXPECT_FALSE(hycast::ChunkId{});
    hycast::ProdInfo prodInfo("product", 0, prodSize);
    hycast::ChunkId chunkInfo{prodInfo, 1};
    EXPECT_TRUE(chunkInfo);
}

// Tests ChunkInfo::equals()
TEST_F(ChunkInfoTest, Equals) {
    hycast::ProdInfo prodInfo("product", 0, prodSize);
    hycast::ChunkId info1(prodInfo, 3);
    EXPECT_TRUE(info1 == info1);
    hycast::ProdInfo prodInfo2("product", 1, prodSize);
    hycast::ChunkId info2(prodInfo2, 3);
    EXPECT_FALSE(info1 == info2);
    hycast::ProdInfo prodInfo3("product", 0, prodSize-1);
    hycast::ChunkId info3(prodInfo3, 3);
    EXPECT_FALSE(info1 == info3);
    hycast::ChunkId info4(prodInfo, 2);
    EXPECT_FALSE(info1 == info4);
}

// Tests serialization/de-serialization
TEST_F(ChunkInfoTest, Serialization) {
    const unsigned version = 0;
    hycast::ProdInfo prodInfo("product", 1, prodSize);
    hycast::ChunkId info1(prodInfo, 2);
    const size_t nbytes = info1.getSerialSize(version);
    alignas(alignof(max_align_t)) char bytes[nbytes];
    hycast::MemEncoder encoder(bytes, nbytes);
    info1.serialize(encoder, version);
    encoder.flush();
    hycast::MemDecoder decoder(bytes, nbytes);
    decoder.fill(0);
    auto info2 = hycast::ChunkId::deserialize(decoder, version);
    EXPECT_TRUE(info1 == info2);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
