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

#include "FixedDelayQueue.h"
#include "ProdStore.h"

#include <fstream>
#include <gtest/gtest.h>

namespace {

// The fixture for testing class ProdStore.
class ProdStoreTest : public ::testing::Test {
protected:
    ProdStoreTest()
    {
        hycast::ChunkInfo::setCanonSize(chunkSize);
    }

    void addChunks(
            const hycast::ProdInfo& prodInfo,
            const char* const       data,
            hycast::ProdStore&      ps)
    {
        for (hycast::ChunkIndex chunkIndex = 0;
                chunkIndex < prodInfo.getNumChunks(); ++chunkIndex) {
            // Serialize chunk
            const auto          chunkInfo = prodInfo.makeChunkInfo(chunkIndex);
            hycast::ActualChunk actualChunk(chunkInfo,
                    data+prodInfo.getOffset(chunkIndex));
            char                buf[actualChunk.getSerialSize(version)];
            hycast::MemEncoder  encoder(buf, sizeof(buf));
            actualChunk.serialize(encoder, version);
            encoder.flush();

            // Create latent chunk and store it
            hycast::MemDecoder  decoder(buf, sizeof(buf));
            decoder.fill(hycast::ChunkInfo::getStaticSerialSize(version));
            hycast::LatentChunk latentChunk(decoder, version);
            hycast::Product     prod;
            const bool isLast = chunkIndex == prodInfo.getNumChunks() - 1;
            EXPECT_EQ(isLast && prod.getInfo().getName().length() > 0,
                    ps.add(latentChunk, prod));
            hycast::ActualChunk actualChunk2;
            EXPECT_TRUE(ps.haveChunk(chunkInfo));
            EXPECT_TRUE(ps.getChunk(chunkInfo, actualChunk2));
            EXPECT_EQ(actualChunk, actualChunk2);
        }
    }

    // Objects declared here can be used by all tests in the test case for ProdStore.
    const unsigned    version{0};
    const std::string pathname{"hycast.ps"};
    hycast::ProdIndex prodIndex{0};
    hycast::ProdSize  prodSize{1};
    hycast::ChunkSize chunkSize{1};
    hycast::ProdInfo  prodInfo{"product", prodIndex, prodSize};
};

// Tests no persistence-file construction
TEST_F(ProdStoreTest, NoPathnameConstruction) {
    hycast::ProdStore ps{};
    EXPECT_EQ(0, ps.size());
}

// Tests persistence-file construction
TEST_F(ProdStoreTest, PathnameConstruction) {
    hycast::ProdStore ps{pathname};
    EXPECT_TRUE(std::ifstream(pathname, std::ifstream::binary).is_open());
    EXPECT_EQ(0, ps.size());
}

// Tests creating an initial entry
TEST_F(ProdStoreTest, InitialEntry) {
    hycast::ProdIndex prodIndex{0};
    hycast::ProdInfo  prodInfo("product", prodIndex, 38000);
    hycast::ProdStore ps{};
    hycast::Product   prod;
    EXPECT_FALSE(ps.add(prodInfo, prod));
    EXPECT_FALSE(prod.isComplete());
    EXPECT_EQ(1, ps.size());
    hycast::ProdInfo prodInfo2;
    EXPECT_TRUE(ps.getProdInfo(prodIndex, prodInfo2));
    EXPECT_EQ(prodInfo, prodInfo2);
    ++prodIndex;
    EXPECT_FALSE(ps.getProdInfo(prodIndex, prodInfo2));
}

// Tests adding latent chunks
TEST_F(ProdStoreTest, AddingLatentChunks) {
    // Create actual chunk
    hycast::ProdIndex         prodIndex{0};
    const char                data[10000] = {'a', 'b'};
    const hycast::ProdSize    prodSize = sizeof(data);
    const hycast::ChunkSize   chunkSize{1000};
    hycast::ChunkInfo::setCanonSize(chunkSize);
    hycast::ProdInfo          prodInfo("product1", prodIndex, prodSize);
    hycast::ProdStore         ps{};

    hycast::Product prod;
    EXPECT_FALSE(ps.add(prodInfo, prod));
    addChunks(prodInfo, data, ps);
    EXPECT_EQ(prodInfo, prod.getInfo());
    EXPECT_EQ(0, ::memcmp(data, prod.getData(), prodSize));
    EXPECT_EQ(1, ps.size());

    ++prodIndex;
    prodInfo = hycast::ProdInfo("product2", prodIndex, prodSize);
    addChunks(prodInfo, data, ps);
    EXPECT_TRUE(ps.add(prodInfo, prod));
    EXPECT_TRUE(prod.isComplete());
    EXPECT_EQ(2, ps.size());
}
// Tests product deletion
TEST_F(ProdStoreTest, ProductDeletion) {
    hycast::ProdInfo  prodInfo("product", 0, 38000);
    hycast::ProdStore ps{"", 0.1};
    hycast::Product   prod;
    EXPECT_FALSE(ps.add(prodInfo, prod));
    EXPECT_EQ(1, ps.size());
    ::sleep(1);
    EXPECT_EQ(0, ps.size());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
