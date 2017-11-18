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
#include "UdpSock.h"

#include <fstream>
#include <gtest/gtest.h>

namespace {

// The fixture for testing class ProdStore.
class ProdStoreTest : public ::testing::Test {
protected:
    ProdStoreTest()
    {
        for (size_t i = 0; i < sizeof(data); ++i)
            data[i] = i % UCHAR_MAX;
        prod = hycast::CompleteProduct{prodIndex, "product", sizeof(data), data};
    }

    void addChunk(
            hycast::ProdStore&        ps,
            const hycast::ProdInfo&   prodInfo,
            const hycast::ChunkIndex  chunkIndex)
    {
        char data[static_cast<size_t>(prodInfo.getChunkSize(chunkIndex))];
        ::memset(data, chunkIndex % UCHAR_MAX, sizeof(data));
        hycast::ActualChunk actualChunk{prodInfo, chunkIndex, data};
        char                buf[actualChunk.getSerialSize(version)];
        hycast::MemEncoder  encoder{buf, sizeof(buf)};
        actualChunk.serialize(encoder, version);
        encoder.flush();

        hycast::MemDecoder  decoder{buf, sizeof(buf)};
        decoder.fill(hycast::LatentChunk::getMetadataSize(version));
        hycast::LatentChunk latentChunk{decoder, version};
        hycast::Product prod{};
        ps.add(latentChunk, prod);
    }

    void addChunks(
            const hycast::ProdInfo prodInfo,
            const char*            data,
            hycast::ProdStore&     ps)
    {
        char                buf[hycast::UdpSock::maxPayload];
        hycast::MemEncoder  encoder(buf, sizeof(buf));
        prodInfo.serialize(encoder, version);
        encoder.flush();

        hycast::MemDecoder  decoder(buf, sizeof(buf));
        decoder.fill();
        auto                prodInfo2 =
                hycast::ProdInfo::deserialize(decoder, version);
        EXPECT_EQ(prodInfo, prodInfo2);

        hycast::Product     prod2{};
        ps.add(prodInfo2, prod2);

        for (hycast::ChunkIndex chunkIndex = 0;
                chunkIndex < prodInfo.getNumChunks(); ++chunkIndex) {
            // Serialize chunk
            const hycast::ChunkInfo   chunkInfo{prodInfo, chunkIndex};
            const hycast::ActualChunk actualChunk{chunkInfo,
                    data+prodInfo.getChunkOffset(chunkIndex)};
            hycast::MemEncoder        encoder(buf, sizeof(buf));
            const auto                nbytes =
                    actualChunk.serialize(encoder, version);
            encoder.flush();

            // Create latent chunk and store it
            hycast::MemDecoder  decoder(buf, nbytes);
            decoder.fill(hycast::LatentChunk::getMetadataSize(version));
            hycast::LatentChunk latentChunk(decoder, version);
            const auto          status = ps.add(latentChunk, prod2);
            const bool          isLast = chunkIndex == prodInfo.getNumChunks() -
                    1;
            EXPECT_EQ(isLast && prod2.getInfo().getName().length() > 0,
                    status.isComplete());
            EXPECT_TRUE(ps.haveChunk(chunkInfo.getId()));
            auto actualChunk2 = ps.getChunk(chunkInfo.getId());
            EXPECT_TRUE(actualChunk2);
            EXPECT_EQ(actualChunk, actualChunk2);
        }
    }

    // Objects declared here can be used by all tests in the test case for ProdStore.
    const unsigned          version{0};
    const std::string       pathname{"hycast.ps"};
    const hycast::ProdIndex prodIndex{0};
    char                    data[128000];
    const hycast::ProdSize  prodSize{sizeof(data)};
    const hycast::ProdInfo  prodInfo{prodIndex, "product", prodSize};
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
    EXPECT_FALSE(ps.add(prodInfo, prod2).isComplete());
    EXPECT_FALSE(prod2.isComplete());
    EXPECT_EQ(1, ps.size());
    auto prodInfo2 = ps.getProdInfo(prodIndex);
    EXPECT_TRUE(prodInfo2);
    EXPECT_EQ(prodInfo, prodInfo2);
    prodInfo2 = ps.getProdInfo(prodIndex+1);
    EXPECT_FALSE(prodInfo2);
}

// Tests adding latent chunks
TEST_F(ProdStoreTest, AddingLatentChunks) {
    hycast::ProdStore ps{};
    hycast::Product   prod2{}; // Empty
    EXPECT_FALSE(ps.add(prodInfo, prod2).isComplete());
    addChunks(prodInfo, prod.getData(), ps);
    EXPECT_EQ(prodInfo, prod2.getInfo());
    EXPECT_EQ(0, ::memcmp(prod.getData(), prod2.getData(), prodSize));
    EXPECT_EQ(1, ps.size());

    hycast::ProdInfo prodInfo2{prodIndex+1, "product2", prodSize};
    addChunks(prodInfo2, prod.getData(), ps);
    EXPECT_TRUE(ps.add(prodInfo2, prod2).isComplete());
    EXPECT_TRUE(prod2.isComplete());
    EXPECT_EQ(2, ps.size());
}

// Tests adding complete product
TEST_F(ProdStoreTest, AddingCompleteProduct) {
    hycast::ProdStore ps{};
    ps.add(prod);
    EXPECT_EQ(1, ps.size());
    auto prodInfo2 = ps.getProdInfo(prodIndex);
    EXPECT_TRUE(prodInfo2);
    EXPECT_EQ(prodInfo, prodInfo2);
}

// Tests product deletion
TEST_F(ProdStoreTest, ProductDeletion) {
    hycast::ProdInfo  prodInfo(0, "product", 38000);
    hycast::ProdStore ps{"", 0.1};
    hycast::Product   prod;
    EXPECT_FALSE(ps.add(prodInfo, prod).isComplete());
    EXPECT_EQ(1, ps.size());
    ::usleep(200000);
    EXPECT_EQ(0, ps.size());
}

// Tests getting information on oldest missing chunk
TEST_F(ProdStoreTest, OldestMissingChunk) {
    hycast::ProdStore ps{""};
    hycast::ProdInfo  prodInfo(0, "Product 0",
            2*hycast::ChunkSize::defaultChunkSize);
    hycast::Product   prod;
    ps.add(prodInfo, prod);
    addChunk(ps, prodInfo, 1);
    hycast::ChunkId chunkId{prodInfo, 0};
    EXPECT_EQ(chunkId, ps.getOldestMissingChunk());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
