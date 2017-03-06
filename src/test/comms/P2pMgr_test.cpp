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

#include <comms/MsgRcvr.h>
#include <comms/MsgRcvrImpl.h>
#include <comms/P2pMgr.h>
#include <comms/Peer.h>
#include "Completer.h"
#include "logging.h"
#include "ProdInfo.h"

#include <future>
#include <gtest/gtest.h>
#include <thread>
#include <unistd.h>

namespace {

// The fixture for testing class P2pMgr.
class P2pMgrTest : public ::testing::Test {
protected:
    class MsgRcvr : public hycast::PeerMsgRcvr {
        hycast::ProdInfo  prodInfo;
        hycast::ChunkInfo chunkInfo;
    public:
        MsgRcvr(
                hycast::ProdInfo& prodInfo,
                hycast::ChunkInfo& chunkInfo)
            : prodInfo{prodInfo}
            , chunkInfo{chunkInfo}
        {}
        void recvNotice(const hycast::ProdInfo& info) {}
        void recvNotice(const hycast::ProdInfo& info, hycast::Peer& peer) {
            EXPECT_EQ(prodInfo, info);
        }
        void recvNotice(const hycast::ChunkInfo& info, hycast::Peer& peer) {
            EXPECT_EQ(chunkInfo, info);
        }
        void recvRequest(const hycast::ProdIndex& index, hycast::Peer& peer) {}
        void recvRequest(const hycast::ChunkInfo& info, hycast::Peer& peer) {}
        void recvData(hycast::LatentChunk chunk) {}
        void recvData(hycast::LatentChunk chunk, hycast::Peer& peer) {}
    };

    P2pMgrTest()
        : prodInfo{"product", 1, 2, 32000}
        , chunkInfo{1, 0}
        , msgRcvr{prodInfo, chunkInfo}
    {}

    hycast::ProdInfo  prodInfo;
    hycast::ChunkInfo chunkInfo;
    MsgRcvr           msgRcvr;
};

// Tests invalid construction argument
TEST_F(P2pMgrTest, InvalidConstruction) {
    hycast::InetSockAddr serverAddr("localhost", 38800);
    EXPECT_THROW(hycast::P2pMgr(serverAddr, 0, nullptr, 60, msgRcvr),
            std::invalid_argument);
}

// Tests construction
TEST_F(P2pMgrTest, Construction) {
    hycast::InetSockAddr serverAddr("localhost", 38800);
    hycast::P2pMgr(serverAddr, 1, nullptr, 60, msgRcvr);
}

// Tests execution
TEST_F(P2pMgrTest, Execution) {
    hycast::InetSockAddr servAddr1("localhost", 38800);
    hycast::InetSockAddr servAddr2("localhost", 38801);
    hycast::P2pMgr p2pMgr1(servAddr1, 1, nullptr, 60, msgRcvr);
    hycast::P2pMgr p2pMgr2(servAddr2, 1, nullptr, 60, msgRcvr);
    hycast::Completer<void> completer{};
    completer.submit([&p2pMgr1]{ p2pMgr1(); });
    completer.submit([&p2pMgr2]{ p2pMgr2(); });
    p2pMgr1.sendNotice(prodInfo);
    p2pMgr1.sendNotice(chunkInfo);
    ::sleep(1);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
