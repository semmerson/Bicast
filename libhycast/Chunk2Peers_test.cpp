/**
 * This file tests the class `Chunk2Peers`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Chunk2Peers_test.cpp
 * @author: Steven R. Emmerson
 */

#include "Chunk2Peers.h"

#include <gtest/gtest.h>

namespace {

// The fixture for testing class Chunk2Peers.
class Chunk2PeersTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    Chunk2PeersTest() {
        map = hycast::Chunk2Peers{};
        prodIndex = hycast::ProdIndex(1);
        info = hycast::ChunkInfo(prodIndex, 2);
        peer = hycast::Peer{};
    }

    // Objects declared here can be used by all tests in the test case for Chunk2Peers.
    hycast::Chunk2Peers map;
    hycast::ProdIndex   prodIndex;
    hycast::ChunkInfo   info;
    hycast::Peer        peer;
};

// Tests default construction
TEST_F(Chunk2PeersTest, DefaultConstruction) {
    hycast::Chunk2Peers map{};
}

// Tests adding a chunk-to-peer mapping
TEST_F(Chunk2PeersTest, Adding) {
    map.add(info, peer);
}

// Tests getting the "front" peer
TEST_F(Chunk2PeersTest, GetFrontPeer) {
    EXPECT_EQ(nullptr, map.getFrontPeer(info));
    map.add(info, peer);
    hycast::Peer* peerPtr = map.getFrontPeer(info);
    EXPECT_NE(nullptr, peerPtr);
    EXPECT_TRUE(peer == *peerPtr);
}

// Tests removing a chunk
TEST_F(Chunk2PeersTest, RemoveChunk) {
    map.add(info, peer);
    map.remove(info);
    EXPECT_EQ(nullptr, map.getFrontPeer(info));
}

// Tests removing a peer with a chunk
TEST_F(Chunk2PeersTest, RemovePeer) {
    map.add(info, peer);
    map.remove(info, peer);
    EXPECT_EQ(nullptr, map.getFrontPeer(info));
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
