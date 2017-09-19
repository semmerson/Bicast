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
        unsigned char data[128000];
        for (size_t i = 0; i < sizeof(data); ++i)
                data[i] = i % UCHAR_MAX;

        prod = hycast::Product{"product", prodIndex, data, sizeof(data)};
    }

    void addChunks(
            const hycast::ProdInfo prodInfo,
            const char*            data,
            hycast::ProdStore&     ps)
    {
        hycast::Product     prod2{};
        for (hycast::ChunkIndex chunkIndex = 0;
                chunkIndex < prodInfo.getNumChunks(); ++chunkIndex) {
            // Serialize chunk
            const auto chunkInfo = prodInfo.makeChunkInfo(chunkIndex);
            hycast::ActualChunk actualChunk(chunkInfo,
                    data+prodInfo.getOffset(chunkIndex));
            char buf[actualChunk.getSerialSize(version)];
            hycast::MemEncoder  encoder(buf, sizeof(buf));
            actualChunk.serialize(encoder, version);
            encoder.flush();

            // Create latent chunk and store it
            hycast::MemDecoder  decoder(buf, sizeof(buf));
            decoder.fill(hycast::ChunkInfo::getStaticSerialSize(version));
            hycast::LatentChunk latentChunk(decoder, version);
            const bool complete = ps.add(latentChunk, prod2);
            const bool isLast = chunkIndex == prodInfo.getNumChunks() - 1;
            EXPECT_EQ(isLast && prod2.getInfo().getName().length() > 0,
                    complete);
            hycast::ActualChunk actualChunk2;
            EXPECT_TRUE(ps.haveChunk(chunkInfo));
            EXPECT_TRUE(ps.getChunk(chunkInfo, actualChunk2));
            EXPECT_EQ(actualChunk, actualChunk2);
        }
    }

    // Objects declared here can be used by all tests in the test case for ProdStore.
    const unsigned          version{0};
    const std::string       pathname{"hycast.ps"};
    const hycast::ProdIndex prodIndex{0};
    const hycast::ProdSize  prodSize{128000};
    const hycast::ProdInfo  prodInfo{"product", prodIndex, prodSize};
    hycast::Product         prod{};
};

// Tests no persistence-file construction
TEST_F(ProdStoreTest, NoPathnameConstruction) {
    hycast::ProdStore ps{};
    EXPECT_EQ(0, ps.size());
}

// Tests persistence-file construction
TEST_F(ProdStoreTest, PathnameConstruction) {
    hycast::ProdStore ps{pathname};
    EXPECT_EQ(0, ps.size());
}

// Tests creating an initial entry
TEST_F(ProdStoreTest, InitialEntry) {
    hycast::ProdStore ps{};
    hycast::Product   prod2;
    EXPECT_FALSE(ps.add(prodInfo, prod2));
    EXPECT_FALSE(prod2.isComplete());
    EXPECT_EQ(1, ps.size());
    hycast::ProdInfo prodInfo2;
    EXPECT_TRUE(ps.getProdInfo(prodIndex, prodInfo2));
    EXPECT_EQ(prodInfo, prodInfo2);
    EXPECT_FALSE(ps.getProdInfo(prodIndex+1, prodInfo2));
}

// Tests adding latent chunks
TEST_F(ProdStoreTest, AddingLatentChunks) {
    hycast::ProdStore ps{};
    hycast::Product   prod2{}; // Empty
    EXPECT_FALSE(ps.add(prodInfo, prod2));
    addChunks(prodInfo, prod.getData(), ps);
    EXPECT_EQ(prodInfo, prod2.getInfo());
    EXPECT_EQ(0, ::memcmp(prod.getData(), prod2.getData(), prodSize));
    EXPECT_EQ(1, ps.size());

    hycast::ProdInfo prodInfo2{"product2", prodIndex+1, prodSize};
    addChunks(prodInfo2, prod.getData(), ps);
    EXPECT_TRUE(ps.add(prodInfo2, prod2));
    EXPECT_TRUE(prod2.isComplete());
    EXPECT_EQ(2, ps.size());
}

// Tests adding complete product
TEST_F(ProdStoreTest, AddingCompleteProduct) {
    hycast::ProdStore ps{};
    ps.add(prod);
    EXPECT_EQ(1, ps.size());
    hycast::ProdInfo prodInfo2;
    EXPECT_TRUE(ps.getProdInfo(prodIndex, prodInfo2));
    EXPECT_EQ(prodInfo, prodInfo2);
}

// Tests product deletion
TEST_F(ProdStoreTest, ProductDeletion) {
    hycast::ProdInfo  prodInfo("product", 0, 38000);
    hycast::ProdStore ps{"", 0.1};
    hycast::Product   prod;
    EXPECT_FALSE(ps.add(prodInfo, prod));
    EXPECT_EQ(1, ps.size());
    ::usleep(200000);
    EXPECT_EQ(0, ps.size());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
