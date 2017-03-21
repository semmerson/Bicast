/**
 * This file tests the UnknownProd class.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: UnknownProd_test.cpp
 * @author: Steven R. Emmerson
 */


#include "UnknownProd.h"

#include <gtest/gtest.h>

namespace {

// The fixture for testing class UnknownProd.
class UnknownProdTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    UnknownProdTest() {
        // You can do set-up work for each test here.
    }

    virtual ~UnknownProdTest() {
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

    // Objects declared here can be used by all tests in the test case for UnknownProd.
};

// Tests construction
TEST_F(UnknownProdTest, Construction) {
    hycast::UnknownProd unkProd();
}

// Tests addition of orphaned chunk
TEST_F(UnknownProdTest, OrphanChunkAddition) {
    // Constants
    const unsigned            version{0};
    const hycast::ProdIndex   prodIndex{0};

    // Data
    char                      data[1000];
    ::memset(data, '\xbd', sizeof(data));
    const hycast::ChunkSize   chunkSize{sizeof(data)};
    hycast::ChunkInfo::setCanonSize(chunkSize);
    const hycast::ProdSize    prodSize{2*chunkSize};
    const hycast::ProdInfo    prodInfo("product", prodIndex, prodSize);

    // Create chunk
    const hycast::ChunkIndex  chunkIndex{1};
    const hycast::ChunkInfo   chunkInfo(prodIndex, prodSize, chunkIndex);
    const hycast::ActualChunk actualChunk(chunkInfo, data);

    // Serialize chunk
    char                      buf[actualChunk.getSerialSize(version)];
    hycast::MemEncoder        encoder(buf, sizeof(buf));
    const auto                nbytes = actualChunk.serialize(encoder, version);
    EXPECT_EQ(sizeof(buf), nbytes);
    encoder.flush();

    // Deserialize chunk
    hycast::MemDecoder        decoder(buf, nbytes);
    decoder.fill(chunkInfo.getSerialSize(version));
    hycast::LatentChunk       latentChunk{decoder, version};

    // Cache chunk
    hycast::UnknownProd       unkProd{};
    ASSERT_TRUE(unkProd.add(latentChunk));

    // Combine with product-information
    auto                      product = unkProd.makeProduct(prodInfo);
    EXPECT_FALSE(product.isComplete());
    EXPECT_EQ(0, ::memcmp(product.getData()+chunkIndex*chunkSize, data,
            chunkSize));
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
