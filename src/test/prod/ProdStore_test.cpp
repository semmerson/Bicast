/**
 * This file tests class `ProdStore`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdStore_test.cpp
 * @author: Steven R. Emmerson
 */


#include "ProdStore.h"

#include <fstream>
#include <gtest/gtest.h>

namespace {

// The fixture for testing class ProdStore.
class ProdStoreTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    ProdStoreTest()
        : pathname("hycast.ps")
    {
        // You can do set-up work for each test here.
    }

    virtual ~ProdStoreTest() {
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

    // Objects declared here can be used by all tests in the test case for ProdStore.
    std::string pathname;
};

// Tests default construction
TEST_F(ProdStoreTest, Construction) {
    hycast::ProdStore{};
}

// Tests pathname construction
TEST_F(ProdStoreTest, PathnameConstruction) {
    hycast::ProdStore{pathname};
    EXPECT_TRUE(std::ifstream(pathname, std::ifstream::binary).is_open());
}

// Tests creating an initial entry
TEST_F(ProdStoreTest, InitialEntry) {
    hycast::ProdInfo  prodInfo("product", 0, 38000, 1000);
    hycast::ProdStore ps{};
    EXPECT_TRUE(ps.add(prodInfo));
    EXPECT_FALSE(ps.add(prodInfo));
}

// Tests adding a latent chunk
TEST_F(ProdStoreTest, AddingLatentChunk) {
    hycast::ProdInfo    prodInfo("product", 0, 1, 1);
    const char          data[] = {'\xbd'};
    hycast::ChunkInfo   chunkInfo(0, 0);
    hycast::ActualChunk actualChunk(chunkInfo, data, sizeof(data));
    char                buf[100];
    hycast::MemEncoder  encoder(buf, sizeof(buf));
    actualChunk.serialize(encoder, 0);
    encoder.flush();
    hycast::MemDecoder  decoder(buf, sizeof(buf));
    decoder.fill(hycast::ChunkInfo::getStaticSerialSize(0));
    hycast::LatentChunk latentChunk(decoder, 0);
    hycast::ProdStore   ps{};
    EXPECT_TRUE(ps.add(prodInfo));
    EXPECT_TRUE(ps.add(latentChunk));
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
