/**
 * This file tests class hycast::Receiving.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Receiving_test.cpp
 * @author: Steven R. Emmerson
 */

#include "P2pMgr.h"
#include "Receiving.h"
#include "YamlPeerSource.h"

#include <gtest/gtest.h>

namespace {

// The fixture for testing class Receiving.
class ReceivingTest : public ::testing::Test, public hycast::PeerMsgRcvr {
protected:
public:
    void recvNotice(const hycast::ProdInfo& info, hycast::Peer& peer)
    {}
    void recvNotice(const hycast::ChunkInfo& info, hycast::Peer& peer)
    {}
    void recvRequest(const hycast::ProdIndex& index, hycast::Peer& peer)
    {}
    void recvRequest(const hycast::ChunkInfo& info, hycast::Peer& peer)
    {}
    void recvData(hycast::LatentChunk chunk, hycast::Peer& peer)
    {}
};

// Tests construction
TEST_F(ReceivingTest, Construction) {
    const hycast::InetSockAddr serverAddr("127.0.0.1", 38801);
    const unsigned maxPeers = 1;
    hycast::YamlPeerSource peerSource("[{inetAddr: 127.0.0.1, port: 38800}]");
    const unsigned stasisDuration = 60;
    hycast::P2pMgr p2pMgr(serverAddr, maxPeers, &peerSource, stasisDuration,
            *this);
    hycast::Receiving{p2pMgr};
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
