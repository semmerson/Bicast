/**
 * This file tests class `PeerAddrSet`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: PeerAddrSet_test.cpp
 * Created On: Dec 7, 2017
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "PeerAddrSet.h"

#include <chrono>
#include <gtest/gtest.h>
#include <random>

namespace {

/// The fixture for testing class `PeerAddrSet`
class PeerAddrSetTest : public ::testing::Test
{
protected:
    hycast::InetAddr     ipv4Addr;
    hycast::InetAddr     ipv6Addr;
    hycast::InetSockAddr peerAddr1;
    hycast::InetSockAddr peerAddr2;

    PeerAddrSetTest()
        : ipv4Addr{"128.117.140.56"}
        , ipv6Addr{"2001:db8::ff00:42:8329"}
        , peerAddr1{ipv4Addr, 38800}
        , peerAddr2{ipv6Addr, 38800}
    {}
};

// Tests default construction
TEST_F(PeerAddrSetTest, DefaultConstruction)
{
    hycast::PeerAddrSet peerAddrSet{};
    EXPECT_EQ(0, peerAddrSet.size());
}

// Tests adding and removing
TEST_F(PeerAddrSetTest, Adding)
{
    hycast::PeerAddrSet peerAddrSet{};
    peerAddrSet.add(peerAddr1);
    EXPECT_EQ(1, peerAddrSet.size());
    peerAddrSet.add(peerAddr1);
    EXPECT_EQ(1, peerAddrSet.size());
    peerAddrSet.add(peerAddr2);
    EXPECT_EQ(2, peerAddrSet.size());
    peerAddrSet.remove(peerAddr1);
    EXPECT_EQ(1, peerAddrSet.size());
    peerAddrSet.remove(peerAddr1);
    EXPECT_EQ(1, peerAddrSet.size());
    peerAddrSet.remove(peerAddr2);
    EXPECT_EQ(0, peerAddrSet.size());
}

// Tests getting random peer-address
TEST_F(PeerAddrSetTest, GetRandom)
{
    hycast::InetSockAddr       peerAddr{};
    hycast::PeerAddrSet        peerAddrSet{};
    std::default_random_engine generator{
            static_cast<std::default_random_engine::result_type>(
                    std::chrono::system_clock::now().time_since_epoch().
                    count())};

    peerAddrSet.add(peerAddr1);
    peerAddrSet.getRandom(peerAddr, generator);
    EXPECT_EQ(peerAddr1, peerAddr);

    peerAddrSet.add(peerAddr2);
    peerAddrSet.getRandom(peerAddr, generator);
    EXPECT_TRUE(peerAddr == peerAddr1 || peerAddr == peerAddr2);

    peerAddrSet.remove(peerAddr1);
    peerAddrSet.getRandom(peerAddr, generator);
    EXPECT_EQ(peerAddr2, peerAddr);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
