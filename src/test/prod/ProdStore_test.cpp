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
    hycast::ProdStore{};
}

// Tests persistence-file construction
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

// Tests adding latent chunks
TEST_F(ProdStoreTest, AddingLatentChunks) {
    // Create actual chunk
    hycast::ProdIndex   prodIndex{0};
    const char          data[10000] = {'a', 'b'};
    hycast::ProdSize    prodSize = sizeof(data);
    hycast::ChunkSize   chunkSize{1000};
    hycast::ProdInfo    prodInfo("product", prodIndex, prodSize, chunkSize);
    hycast::ProdStore   ps{};
    EXPECT_TRUE(ps.add(prodInfo));
    hycast::Product     prod;

    for (hycast::ChunkIndex chunkIndex = 0;
            chunkIndex < prodInfo.getNumChunks(); ++chunkIndex) {
        // Serialize chunk
        const auto          chunkInfo = prodInfo.makeChunkInfo(chunkIndex);
        hycast::ActualChunk actualChunk(chunkInfo, data+chunkIndex*chunkSize);
        char                buf[actualChunk.getSerialSize(version)];
        hycast::MemEncoder  encoder(buf, sizeof(buf));
        actualChunk.serialize(encoder, version);
        encoder.flush();

        // Create latent chunk and store it
        hycast::MemDecoder  decoder(buf, sizeof(buf));
        decoder.fill(hycast::ChunkInfo::getStaticSerialSize(version));
        hycast::LatentChunk latentChunk(decoder, version);
        EXPECT_TRUE(ps.add(latentChunk, prod));
        EXPECT_EQ(chunkIndex == prodInfo.getNumChunks()-1, prod.isComplete());

        // Attempt to store duplicate latent chunk
        hycast::MemDecoder  decoder2(buf, sizeof(buf));
        decoder2.fill(hycast::ChunkInfo::getStaticSerialSize(version));
        hycast::LatentChunk latentChunk2(decoder2, version);
        EXPECT_FALSE(ps.add(latentChunk2, prod));
    }
    EXPECT_EQ(prodInfo, prod.getInfo());
    EXPECT_EQ(0, ::memcmp(data, prod.getData(), prodSize));
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
