/**
 * This file tests the class `P2pMgr`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: P2pMgr_test.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "Completer.h"
#include "error.h"
#include "Interface.h"
#include "P2pMgr.h"
#include "Peer.h"
#include "PeerMsgRcvr.h"
#include "PeerSet.h"
#include "ProdInfo.h"
#include "ProdStore.h"
#include "Shipping.h"
#include "YamlPeerSource.h"

#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <unistd.h>

namespace {

// The fixture for testing class P2pMgr.
class P2pMgrTest : public ::testing::Test, public hycast::PeerMsgRcvr {
protected:
    P2pMgrTest()
    {
        for (size_t i = 0; i < sizeof(data); ++i)
                data[i] = i % UCHAR_MAX;
        prod = hycast::CompleteProduct{prodIndex, "product", sizeof(data),
                data};
        auto peerAddr = hycast::InetSockAddr(srcInetAddr, srcPort);
    }

    /**
     * Runs the peer-to-peer manager. Intended to run on its own thread.
     */
    static void runP2pMgr(hycast::P2pMgr& p2pMgr)
    {
    	try {
            p2pMgr.run();
            throw hycast::LOGIC_ERROR("Peer-to-peer manager stopped");
    	}
    	catch (const std::exception& e) {
            hycast::log_error(e);
    	}
    }

    hycast::ProdStore               prodStore{};
    hycast::InetSockAddr            mcastAddr{"232.0.0.0", 38800};
    const unsigned                  protoVers{0};
    const in_port_t                 srcPort{38800};
    const in_port_t                 snkPort = srcPort + 1;
    const std::string               srcInetAddr{"127.0.0.1"};
    const hycast::InetSockAddr      srcSrvrAddr{srcInetAddr, srcPort};
    const hycast::InetSockAddr      snkSrvrAddr{
            hycast::Interface{ETHNET_IFACE_NAME}.getInetAddr(AF_INET), snkPort};
    const unsigned                  maxPeers{2};
    hycast::YamlPeerSource          peerSource{"[{inetAddr: " + srcInetAddr +
    	    ", port: " + std::to_string(srcPort) + "}]"};
    const hycast::PeerSet::TimeUnit stasisDuration{2};
    hycast::ProdIndex               prodIndex{0};
    char                            data[3000];
    hycast::ProdInfo                prodInfo{prodIndex, "product",
                                            sizeof(data)};
    hycast::Product                 prod{};
    std::atomic_long                numRcvdChunks{0};

public:
    void recvNotice(const hycast::ProdInfo& info, const hycast::Peer& peer)
    {
        EXPECT_EQ(prodInfo, info);
    }

    void recvNotice(const hycast::ChunkId& chunkId, const hycast::Peer& peer)
    {
    	EXPECT_EQ(prodIndex, chunkId.getProdIndex());
    	EXPECT_TRUE(chunkId.getChunkIndex() < prodInfo.getNumChunks());
        peer.sendRequest(chunkId);
    }

    void recvRequest(const hycast::ProdIndex& index, const hycast::Peer& peer)
    {
    	GTEST_FAIL();
    }

    void recvRequest(const hycast::ChunkId& chunkId, const hycast::Peer& peer)
    {
    	GTEST_FAIL();
    }

    void recvData(hycast::LatentChunk chunk, const hycast::Peer& peer)
    {
    	hycast::ChunkInfo chunkInfo{chunk.getInfo()};
    	EXPECT_EQ(prodIndex, chunkInfo.getProdIndex());
    	EXPECT_TRUE(chunkInfo.getIndex() < prodInfo.getNumChunks());
    	unsigned char data[static_cast<size_t>(chunk.getSize())];
    	chunk.drainData(data, sizeof(data));
    	EXPECT_EQ(0, ::memcmp(data, this->data+chunk.getOffset(),
    			sizeof(data)));
    	++numRcvdChunks;
    }
};

// Tests construction
TEST_F(P2pMgrTest, Construction) {
    hycast::InetSockAddr serverAddr("localhost", 38800);
    hycast::ProdStore prodStore{};
    hycast::P2pMgr{prodStore, serverAddr, *this, peerSource};
}

// Tests execution
TEST_F(P2pMgrTest, Execution) {
    hycast::Shipping  shipping{prodStore, mcastAddr, protoVers, srcSrvrAddr};
    hycast::ProdStore prodStore{};
    hycast::P2pMgr    p2pMgr{prodStore, snkSrvrAddr, *this, peerSource,
            maxPeers, stasisDuration};
    std::thread       p2pMgrThread{[this,&p2pMgr]{runP2pMgr(p2pMgr);}};
    ::sleep(1);
    shipping.ship(prod);
    ::sleep(1);
    ::pthread_cancel(p2pMgrThread.native_handle());
    p2pMgrThread.join();
    std::cerr << "Chunks received: " << numRcvdChunks << '\n';
    EXPECT_EQ(prodInfo.getNumChunks(), numRcvdChunks);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
