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
#include "PeerSet.h"
#include "ProdInfo.h"
#include "ProdStore.h"
#include "Thread.h"
#include "YamlPeerSource.h"

#include <atomic>
#include <gtest/gtest.h>
#include <unistd.h>

namespace {

// The fixture for testing class P2pMgr.
class P2pMgrTest : public ::testing::Test, public hycast::P2pMgrServer {
protected:
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
    hycast::YamlPeerSource          snkPeerSource{"[{inetAddr: " + srcInetAddr +
    	    ", port: " + std::to_string(srcPort) + "}]"};
    const unsigned                  stasisDuration{2};
    hycast::ProdIndex               prodIndex{1};
    hycast::ProdSize                prodSize;
    char*                           prodData;
    hycast::ProdInfo                prodInfo{prodIndex, "product", prodSize};
    hycast::ChunkIndex              numChunks = prodInfo.getNumChunks();
    hycast::Product                 prod{};
    hycast::Cue                     cue{};
    std::atomic_long                numRcvdChunks{0};

    P2pMgrTest()
        : prodSize{1000000}
        , prodData{new char[prodSize]}
    {
        for (size_t i = 0; i < prodSize; ++i)
                prodData[i] = i % UCHAR_MAX;
        prod = hycast::MemoryProduct{prodIndex, "product", prodSize,
                prodData};
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

    void send(
            hycast::P2pMgr&         p2pMgr,
            const hycast::ProdInfo& prodInfo)
    {
        p2pMgr.notify(prodInfo.getIndex());
        auto numChunks = prodInfo.getNumChunks();
        for (hycast::ChunkIndex chunkIndex = 0;
                chunkIndex < numChunks; ++chunkIndex) {
            try {
                hycast::ChunkId chunkId{prodInfo, chunkIndex};
                p2pMgr.notify(chunkId);
            }
            catch (const std::exception& ex) {
                std::throw_with_nested(hycast::RUNTIME_ERROR(
                        "Couldn't notify about chunk " +
                        chunkIndex.to_string()));
            }
        }
    }

public:
    // Begin implementation of `PeerSetServer` interface

    hycast::ChunkId getEarliestMissingChunkId() {
        return hycast::ChunkId{};
    }

    hycast::Backlogger getBacklogger(
            const hycast::ChunkId& earliest,
            hycast::PeerMsgSndr&          peer)
    {
        return hycast::Backlogger(peer, earliest, prodStore);
    }
    bool shouldRequest(const hycast::ProdIndex& index)
    {
        EXPECT_EQ(prodIndex, index);
        return true;
    }
    bool shouldRequest(const hycast::ChunkId& chunkId)
    {
        EXPECT_EQ(prodIndex, chunkId.getProdIndex());
        EXPECT_GT(numChunks, chunkId.getChunkIndex());
        return true;
    }
    bool get(const hycast::ProdIndex& index, hycast::ProdInfo& info)
    {
        EXPECT_EQ(prodIndex, index);
        info = prodInfo;
        return true;
    }
    bool get(const hycast::ChunkId& chunkId, hycast::ActualChunk& chunk)
    {
        EXPECT_EQ(prodIndex, chunkId.getProdIndex());
        auto chunkIndex = chunkId.getChunkIndex();
        EXPECT_GT(numChunks, chunkIndex);
        const hycast::ChunkInfo chunkInfo{prodInfo, chunkId.getChunkIndex()};
        chunk = hycast::ActualChunk(chunkInfo, prodData + chunkInfo.getOffset());
        return true;
    }
    hycast::RecvStatus receive(
            const hycast::ProdInfo&     info,
            const hycast::InetSockAddr& peerAddr)
    {
        EXPECT_EQ(prodInfo, info);
        EXPECT_EQ(srcSrvrAddr, peerAddr);
        return hycast::RecvStatus{};
    }
    hycast::RecvStatus receive(
            hycast::InetChunk&        chunk,
            const hycast::InetSockAddr& peerAddr)
    {
        EXPECT_EQ(srcSrvrAddr, peerAddr);
        auto chunkIndex = chunk.getIndex();
        EXPECT_GT(numChunks, chunkIndex);
        EXPECT_EQ(prodIndex, chunk.getProdIndex());
        EXPECT_EQ(prodSize, chunk.getProdSize());
        EXPECT_EQ(hycast::ChunkSize::defaultSize, chunk.getCanonSize());
        auto chunkSize = chunk.getSize();
        EXPECT_EQ(prodInfo.getChunkSize(chunkIndex), chunkSize);
        char chunkData[static_cast<size_t>(chunkSize)];
        chunk.drainData(chunkData, chunkSize);
        // Most time here
        EXPECT_EQ(0, ::memcmp(prodData+chunk.getOffset(), chunkData, chunkSize));
        LOG_INFO("Received chunk " + chunkIndex.to_string());
        if (++numRcvdChunks == numChunks)
            cue.cue();
        return hycast::RecvStatus{};
    }
};

// Tests construction
TEST_F(P2pMgrTest, Construction) {
    hycast::InetSockAddr serverAddr("localhost", 38800);
    hycast::P2pMgr{serverAddr, *this, snkPeerSource};
}

// Tests execution
TEST_F(P2pMgrTest, Execution) {
    auto srcPeerSource = hycast::NilPeerSource{};
    hycast::P2pMgr    srcP2pMgr{srcSrvrAddr, *this, srcPeerSource};
    hycast::P2pMgr    snkP2pMgr{snkSrvrAddr, *this, snkPeerSource, maxPeers,
            stasisDuration};
    hycast::Thread    srcp2pMgrThread{[this,&srcP2pMgr]{runP2pMgr(srcP2pMgr);}};
    hycast::Thread    snkp2pMgrThread{[this,&snkP2pMgr]{runP2pMgr(snkP2pMgr);}};
    usleep(1000000);
    send(srcP2pMgr, prodInfo);
    cue.wait();
    std::cerr << "Chunks received: " << numRcvdChunks << '\n';
    EXPECT_EQ(prodInfo.getNumChunks(), numRcvdChunks);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
