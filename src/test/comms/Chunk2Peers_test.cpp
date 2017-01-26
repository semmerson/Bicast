/**
 * This file tests the class `MapOfLists`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Chunk2Peers_test.cpp
 * @author: Steven R. Emmerson
 */

//#include "Chunk2Peers.h"
#include <comms/Peer.h>
#include "ChunkInfo.h"
#include "MapOfLists.h"
#include <gtest/gtest.h>

namespace hycast {
    template class MapOfLists<ChunkInfo, Peer>;
}

namespace {

// The fixture for testing a map from chunk information to a list of peers
class Chunk2PeersTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    Chunk2PeersTest() {
        prodIndex = hycast::ProdIndex(1);
        info = hycast::ChunkInfo(prodIndex, 2);
        peer = hycast::Peer{};
    }

    // Objects declared here can be used by all tests in the test case for Chunk2Peers.
    //hycast::Chunk2Peers map;
    hycast::MapOfLists<hycast::ChunkInfo, hycast::Peer> map{};
    hycast::ProdIndex   prodIndex{};
    hycast::ChunkInfo   info{};
    hycast::Peer        peer{};
};

// Tests default construction
TEST_F(Chunk2PeersTest, DefaultConstruction) {
    //hycast::Chunk2Peers map{};
    hycast::MapOfLists<hycast::ChunkInfo, hycast::Peer> map{};
}

// Tests adding a chunk-to-peer mapping
TEST_F(Chunk2PeersTest, Adding) {
    map.add(info, peer);
}

// Tests getting the first peer
TEST_F(Chunk2PeersTest, GetFrontPeer) {
    //auto bounds = map.getPeers(info);
    auto bounds = map.getValues(info);
    EXPECT_EQ(bounds.first, bounds.second);
    map.add(info, peer);
    //bounds = map.getPeers(info);
    bounds = map.getValues(info);
    EXPECT_EQ(peer, *bounds.first);
    bounds.first++;
    EXPECT_EQ(bounds.first, bounds.second);
}

// Tests removing a chunk
TEST_F(Chunk2PeersTest, RemoveChunk) {
    map.add(info, peer);
    map.remove(info);
    //auto bounds = map.getPeers(info);
    auto bounds = map.getValues(info);
    EXPECT_EQ(bounds.first, bounds.second);
}

// Tests removing a peer with a chunk
TEST_F(Chunk2PeersTest, RemovePeer) {
    map.add(info, peer);
    map.remove(info, peer);
    //auto bounds = map.getPeers(info);
    auto bounds = map.getValues(info);
    EXPECT_EQ(bounds.first, bounds.second);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
