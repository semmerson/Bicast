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

#include "Completer.h"
#include "error.h"
#include "logging.h"
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
        prod = hycast::Product{"product", prodIndex, data, sizeof(data)};
        auto peerAddr = hycast::InetSockAddr(srvrInetAddr, srcPort);
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
    const std::string               srvrInetAddr{"127.0.0.1"};
    const hycast::InetSockAddr      srcSrvrAddr{srvrInetAddr, srcPort};
    const hycast::InetSockAddr      snkSrvrAddr{srvrInetAddr, snkPort};
    const unsigned                  maxPeers{2};
    hycast::YamlPeerSource          peerSource{"[{inetAddr: " + srvrInetAddr +
    	    ", port: " + std::to_string(srcPort) + "}]"};
    const hycast::PeerSet::TimeUnit stasisDuration{2};
    hycast::ProdIndex               prodIndex{0};
    unsigned char                   data[3000];
    hycast::ProdInfo                prodInfo{"product", prodIndex,
                                            sizeof(data)};
    hycast::Product                 prod{};
    std::atomic_long                numRcvdChunks{0};

public:
    void recvNotice(const hycast::ProdInfo& info, const hycast::Peer& peer)
    {
        EXPECT_EQ(prodInfo, info);
    }

    void recvNotice(const hycast::ChunkInfo& info, const hycast::Peer& peer)
    {
    	EXPECT_EQ(prodIndex, info.getProdIndex());
    	EXPECT_TRUE(info.getIndex() < prodInfo.getNumChunks());
        peer.sendRequest(info);
    }

    void recvRequest(const hycast::ProdIndex& index, const hycast::Peer& peer)
    {
    	GTEST_FAIL();
    }

    void recvRequest(const hycast::ChunkInfo& info, const hycast::Peer& peer)
    {
    	GTEST_FAIL();
    }

    void recvData(hycast::LatentChunk chunk, const hycast::Peer& peer)
    {
    	hycast::ChunkInfo chunkInfo{chunk.getInfo()};
    	EXPECT_EQ(prodIndex, chunkInfo.getProdIndex());
    	EXPECT_TRUE(chunkInfo.getIndex() < prodInfo.getNumChunks());
    	unsigned char data[chunk.getSize()];
    	chunk.drainData(data, sizeof(data));
    	EXPECT_EQ(0, ::memcmp(data, this->data+chunk.getOffset(),
    			sizeof(data)));
    	++numRcvdChunks;
    }
};

// Tests construction
TEST_F(P2pMgrTest, Construction) {
    hycast::InetSockAddr serverAddr("localhost", 38800);
    hycast::P2pMgr(serverAddr, *this, peerSource);
}

// Tests execution
TEST_F(P2pMgrTest, Execution) {
    hycast::Shipping shipping{prodStore, mcastAddr, protoVers, srcSrvrAddr};
    hycast::P2pMgr   p2pMgr{snkSrvrAddr, *this, peerSource, maxPeers,
            stasisDuration};
    std::thread p2pMgrThread{[&]{runP2pMgr(p2pMgr);}};
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
