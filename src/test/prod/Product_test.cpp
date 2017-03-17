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
    hycast::ProdInfo info("product", 0, 2, 1); // 2 chunks of 1-byte each
    hycast::Product prod{info};
    EXPECT_FALSE(prod.isComplete());
    EXPECT_EQ(info, prod.getInfo());
}

// Tests adding chunks
TEST_F(ProductTest, AddChunk) {
    char              data[] = {'a', 'b'};
    hycast::ProdIndex prodIndex{0};
    hycast::ProdSize  prodSize{sizeof(data)};
    hycast::ChunkSize chunkSize{1};
    hycast::ProdInfo  info("product", prodIndex, prodSize, chunkSize);
    hycast::Product   prod{info};
    hycast::ActualChunk actualChunk{info.makeChunkInfo(0), data};
    EXPECT_TRUE(prod.add(actualChunk));
    EXPECT_FALSE(prod.isComplete());
    EXPECT_FALSE(prod.add(actualChunk));
    actualChunk = hycast::ActualChunk(info.makeChunkInfo(1), data+chunkSize);
    EXPECT_TRUE(prod.add(actualChunk));
    EXPECT_TRUE(prod.isComplete());
    EXPECT_EQ(prodSize, prod.getInfo().getSize());
    EXPECT_EQ(0, ::memcmp(data, prod.getData(), prodSize));
}

// Tests serialization
TEST_F(ProductTest, Serialization) {
    unsigned           version{0};
    hycast::ProdIndex  prodIndex{0};
    char               data[] = {'a', 'b'};
    hycast::ProdSize   prodSize{sizeof(data)};
    hycast::ChunkSize  chunkSize{1};
    hycast::ProdInfo   prodInfo("product", prodIndex, prodSize, chunkSize);
    hycast::Product    prod{prodInfo};
    for (hycast::ChunkIndex chunkIndex = 0;
            chunkIndex < prodInfo.getNumChunks(); ++chunkIndex) {
        hycast::ChunkInfo   chunkInfo{prodInfo.makeChunkInfo(chunkIndex)};
        hycast::ActualChunk actualChunk(chunkInfo, data + chunkIndex*chunkSize);
        char                buf[actualChunk.getSerialSize(version)];
        hycast::MemEncoder  encoder(buf, sizeof(buf));
        size_t              nbytes = actualChunk.serialize(encoder, version);
        EXPECT_EQ(sizeof(buf), nbytes);
        encoder.flush();
        hycast::MemDecoder decoder(buf, nbytes);
        decoder.fill(hycast::ChunkInfo::getStaticSerialSize(version));
        hycast::LatentChunk latentChunk{decoder, version};
        EXPECT_EQ(chunkInfo, latentChunk.getInfo());
        char latentData[latentChunk.getSize()];
        EXPECT_EQ(chunkSize, latentChunk.drainData(latentData,
                sizeof(latentData)));
        EXPECT_EQ(0, ::memcmp(data+chunkIndex*chunkSize, latentData, chunkSize));
    }
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
