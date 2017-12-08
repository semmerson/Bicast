/**
 * This file tests class `ChunkId2PeerAddrsMap`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: ChunkId2PeerAddrsMap_test.cpp
 * Created On: Dec 7, 2017
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "ChunkId2PeerAddrsMap.h"
#include "ProdInfo.h"

#include <chrono>
#include <gtest/gtest.h>
#include <random>

namespace {

/// The fixture for testing class `ChunkId2PeerAddrsMap`
class ChunkId2PeerAddrsMapTest : public ::testing::Test
{
protected:
    hycast::ProdInfo     prodInfo;
    hycast::ChunkId      chunkId1;
    hycast::ChunkId      chunkId2;
    hycast::InetAddr     ipv4Addr;
    hycast::InetAddr     ipv6Addr;
    hycast::InetSockAddr peerAddr1;
    hycast::InetSockAddr peerAddr2;

    ChunkId2PeerAddrsMapTest()
        : prodInfo(1, "product", 100000)
        , chunkId1(prodInfo, 0)
        , chunkId2(prodInfo, 1)
        , ipv4Addr{"128.117.140.56"}
        , ipv6Addr{"2001:db8::ff00:42:8329"}
        , peerAddr1{ipv4Addr, 38800}
        , peerAddr2{ipv6Addr, 38800}
    {}
};

// Tests default construction
TEST_F(ChunkId2PeerAddrsMapTest, DefaultConstruction)
{
    hycast::ChunkId2PeerAddrsMap chunkId2PeerAddrsMap{};
    EXPECT_EQ(0, chunkId2PeerAddrsMap.size());
}

// Tests adding and removing
TEST_F(ChunkId2PeerAddrsMapTest, Adding)
{
    hycast::ChunkId2PeerAddrsMap peerAddrs{};
    EXPECT_EQ(0, peerAddrs.size());

    peerAddrs.add(chunkId1, peerAddr1);
    EXPECT_EQ(1, peerAddrs.size());

    peerAddrs.add(chunkId1, peerAddr1);
    EXPECT_EQ(1, peerAddrs.size());

    peerAddrs.add(chunkId1, peerAddr2);
    EXPECT_EQ(1, peerAddrs.size());

    peerAddrs.add(chunkId2, peerAddr1);
    EXPECT_EQ(2, peerAddrs.size());

    peerAddrs.remove(chunkId1, peerAddr1);
    EXPECT_EQ(2, peerAddrs.size());

    peerAddrs.remove(chunkId2, peerAddr2);
    EXPECT_EQ(2, peerAddrs.size());

    peerAddrs.remove(chunkId1);
    EXPECT_EQ(1, peerAddrs.size());
}

// Tests getting random peer-address
TEST_F(ChunkId2PeerAddrsMapTest, GetRandom)
{
    hycast::InetSockAddr         peerAddr{};
    hycast::ChunkId2PeerAddrsMap peerAddrs{};
    std::default_random_engine generator{
            static_cast<std::default_random_engine::result_type>(
                    std::chrono::system_clock::now().time_since_epoch().
                    count())};

    ASSERT_FALSE(peerAddrs.getRandom(chunkId1, peerAddr, generator));

    peerAddrs.add(chunkId1, peerAddr1);
    ASSERT_TRUE(peerAddrs.getRandom(chunkId1, peerAddr, generator));
    EXPECT_EQ(peerAddr1, peerAddr);

    EXPECT_FALSE(peerAddrs.getRandom(chunkId2, peerAddr, generator));

    peerAddrs.add(chunkId1, peerAddr2);
    ASSERT_TRUE(peerAddrs.getRandom(chunkId1, peerAddr, generator));
    EXPECT_TRUE(peerAddr == peerAddr1 || peerAddr == peerAddr2);

    peerAddrs.remove(chunkId1, peerAddr1);
    ASSERT_TRUE(peerAddrs.getRandom(chunkId1, peerAddr, generator));
    EXPECT_EQ(peerAddr2, peerAddr);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
