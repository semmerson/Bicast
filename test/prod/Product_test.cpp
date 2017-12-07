/**
 * This file tests the class `Product`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Product_test.cpp
 * @author: Steven R. Emmerson
 */


#include <Chunk.h>
#include "Product.h"

#include <gtest/gtest.h>

namespace {

// The fixture for testing class Product.
class ProductTest : public ::testing::Test {
protected:
    // Objects declared here can be used by all tests in the test case for Product.
};

// Tests construction from product-information
TEST_F(ProductTest, ProdInfoConstruction) {
    // 2 chunks
    hycast::ProdInfo info{0, "product", 2*hycast::ChunkSize::defaultSize};
    hycast::PartialProduct prod{info};
    EXPECT_FALSE(prod.isComplete());
    EXPECT_EQ(info, prod.getInfo());
    EXPECT_FALSE(prod.getChunk(0));
}

// Tests adding chunks
TEST_F(ProductTest, AddChunk) {
    char                   data[2][hycast::ChunkSize::defaultSize];
    hycast::ProdSize       prodSize{sizeof(data)};
    hycast::ProdIndex      prodIndex{0};
    hycast::ProdInfo       info{prodIndex, "product", prodSize};
    hycast::PartialProduct prod{info};
    EXPECT_FALSE(prod.getChunk(0));

    hycast::ActualChunk    actualChunk{info, 0, data[0]};
    EXPECT_TRUE(prod.add(actualChunk));
    EXPECT_FALSE(prod.isComplete());
    EXPECT_FALSE(prod.add(actualChunk));
    auto actualChunk2 = prod.getChunk(0);
    EXPECT_TRUE(actualChunk2);
    EXPECT_EQ(actualChunk, actualChunk2);

    actualChunk = hycast::ActualChunk{info, 1, data[1]};
    EXPECT_TRUE(prod.add(actualChunk));
    EXPECT_TRUE(prod.isComplete());
    EXPECT_EQ(prodSize, prod.getInfo().getSize());
    actualChunk2 = prod.getChunk(1);
    EXPECT_TRUE(actualChunk2);
    EXPECT_EQ(actualChunk, actualChunk2);

    EXPECT_EQ(0, ::memcmp(data, prod.getData(), prodSize));
}

// Tests identifying the earliest missing chunk of data
TEST_F(ProductTest, IdentifyEarliestMissingChunk) {
    char                   data[2][hycast::ChunkSize::defaultSize];
    hycast::ProdIndex      prodIndex{0};
    hycast::ProdSize       prodSize{sizeof(data)};
    hycast::ProdInfo       prodInfo{prodIndex, "product", prodSize};
    hycast::PartialProduct prod{prodInfo};

    // Empty product:
    auto chunkId = prod.identifyEarliestMissingChunk();
    hycast::ChunkId expect(prodInfo, 0);
    EXPECT_EQ(expect, chunkId);

    // Add first chunk:
    hycast::ActualChunk actualChunk{prodInfo, 0, data[0]};
    prod.add(actualChunk);
    chunkId = prod.identifyEarliestMissingChunk();
    expect = hycast::ChunkId{prodInfo, 1};
    EXPECT_EQ(expect, chunkId);

    // Add last chunk:
    actualChunk = hycast::ActualChunk{prodInfo, 1, data[1]};
    prod.add(actualChunk);
    chunkId = prod.identifyEarliestMissingChunk();
    EXPECT_FALSE(chunkId);
}

// Tests construction from complete data
TEST_F(ProductTest, DataConstruction) {
    char                    data[128000];
    ::memset(data, 0xbd, sizeof(data));
    hycast::CompleteProduct prod{1, "product", sizeof(data), data};
    auto                    prodInfo = prod.getInfo();
    EXPECT_STREQ("product", prodInfo.getName().c_str());
    EXPECT_EQ(1, prodInfo.getIndex());
    ASSERT_EQ(sizeof(data), prodInfo.getSize());
    EXPECT_EQ(0, ::memcmp(data, prod.getData(), sizeof(data)));
}

// Tests serialization
TEST_F(ProductTest, Serialization) {
    unsigned               version{0};
    hycast::ProdIndex      prodIndex{0};
    const auto             chunkSize = hycast::ChunkSize::defaultSize;
    char                   data[2][chunkSize];
    hycast::ProdSize       prodSize{sizeof(data)};
    hycast::ProdInfo       prodInfo{prodIndex, "product", prodSize};
    hycast::PartialProduct prod{prodInfo};
    for (hycast::ChunkIndex chunkIndex = 0;
            chunkIndex < prodInfo.getNumChunks(); ++chunkIndex) {
        hycast::ChunkId     chunkId{prodInfo, chunkIndex};
        hycast::ActualChunk actualChunk{prodInfo, chunkIndex, data[chunkIndex]};
        char                buf[actualChunk.getSerialSize(version)];
        hycast::MemEncoder  encoder(buf, sizeof(buf));
        size_t              nbytes = actualChunk.serialize(encoder, version);
        EXPECT_EQ(sizeof(buf), nbytes);
        encoder.flush();
        hycast::MemDecoder decoder(buf, nbytes);
        decoder.fill(hycast::LatentChunk::getMetadataSize(version));
        hycast::LatentChunk latentChunk{decoder, version};
        EXPECT_EQ(chunkId, latentChunk.getId());
        char latentData[static_cast<size_t>(latentChunk.getSize())];
        EXPECT_EQ(chunkSize, latentChunk.drainData(latentData,
                sizeof(latentData)));
        EXPECT_EQ(0, ::memcmp(data[chunkIndex], latentData, chunkSize));
    }
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
