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
class ProdStoreTest : public ::testing::Test, public hycast::ProdRcvr {
protected:
    void recvProd(hycast::Product& prod)
    {
        EXPECT_EQ(this->prod, prod);
    }

    // Objects declared here can be used by all tests in the test case for ProdStore.
    const unsigned    version{0};
    const std::string pathname{"hycast.ps"};
    hycast::ProdIndex prodIndex{0};
    hycast::ProdSize  prodSize{1};
    hycast::ChunkSize chunkSize{1};
    hycast::ProdInfo  prodInfo{"product", prodIndex, prodSize, chunkSize};
    hycast::Product   prod{prodInfo};
};

// Tests no persistence-file construction
TEST_F(ProdStoreTest, NoPathnameConstruction) {
    hycast::ProdStore{*this};
}

// Tests persistence-file construction
TEST_F(ProdStoreTest, PathnameConstruction) {
    hycast::ProdStore{*this, pathname};
    EXPECT_TRUE(std::ifstream(pathname, std::ifstream::binary).is_open());
}

// Tests creating an initial entry
TEST_F(ProdStoreTest, InitialEntry) {
    hycast::ProdInfo  prodInfo("product", 0, 38000, 1000);
    hycast::ProdStore ps{*this};
    EXPECT_TRUE(ps.add(prodInfo));
    EXPECT_FALSE(ps.add(prodInfo));
}

// Tests adding a latent chunk
TEST_F(ProdStoreTest, AddingLatentChunk) {
    // Create actual chunk
    hycast::ProdIndex   prodIndex{0};
    const char          data[] = {'\xbd'};
    hycast::ProdSize    prodSize = sizeof(data);
    hycast::ProdInfo    prodInfo("product", prodIndex, prodSize, prodSize);
    const auto          chunkInfo = prodInfo.makeChunkInfo(0);
    hycast::ActualChunk actualChunk(chunkInfo, data);

    // Serialize chunk
    char                buf[actualChunk.getSerialSize(version)];
    hycast::MemEncoder  encoder(buf, sizeof(buf));
    actualChunk.serialize(encoder, version);
    encoder.flush();

    // De-serialize chunk and store it
    hycast::MemDecoder  decoder(buf, sizeof(buf));
    decoder.fill(hycast::ChunkInfo::getStaticSerialSize(version));
    hycast::LatentChunk latentChunk(decoder, version);
    hycast::ProdStore   ps{*this};
    EXPECT_TRUE(ps.add(prodInfo));
    ps.add(latentChunk);

    // Check attempt to store duplicate chunk
    hycast::MemDecoder  decoder2(buf, sizeof(buf));
    decoder2.fill(hycast::ChunkInfo::getStaticSerialSize(version));
    hycast::LatentChunk latentChunk2(decoder2, version);
    ps.add(latentChunk2);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
