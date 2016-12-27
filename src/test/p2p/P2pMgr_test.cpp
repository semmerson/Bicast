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
#include "logging.h"
#include "MsgRcvr.h"
#include "MsgRcvrImpl.h"
#include "P2pMgr.h"
#include "Peer.h"
#include "ProdInfo.h"
#include "Task.h"

#include <future>
#include <gtest/gtest.h>
#include <thread>
#include <unistd.h>

namespace {

// The fixture for testing class P2pMgr.
class P2pMgrTest : public ::testing::Test {
protected:
    struct MsgRcvr : public hycast::MsgRcvr {
        void recvNotice(const hycast::ProdInfo& info, hycast::Peer& peer) {}
        void recvNotice(const hycast::ChunkInfo& info, hycast::Peer& peer) {}
        void recvRequest(const hycast::ProdIndex& index, hycast::Peer& peer) {}
        void recvRequest(const hycast::ChunkInfo& info, hycast::Peer& peer) {}
        void recvData(hycast::LatentChunk chunk, hycast::Peer& peer) {}
    };
};

// Tests invalid construction argument
TEST_F(P2pMgrTest, InvalidConstruction) {
    hycast::InetSockAddr serverAddr("localhost", 38800);
    MsgRcvr msgRcvr{};
    EXPECT_THROW(hycast::P2pMgr(serverAddr, 0, nullptr, 60, msgRcvr),
            std::invalid_argument);
}

// Tests construction
TEST_F(P2pMgrTest, Construction) {
    hycast::InetSockAddr serverAddr("localhost", 38800);
    MsgRcvr msgRcvr{};
    hycast::P2pMgr(serverAddr, 1, nullptr, 60, msgRcvr);
}

// Tests execution
TEST_F(P2pMgrTest, Execution) {
    hycast::InetSockAddr servAddr1("localhost", 38800);
    hycast::InetSockAddr servAddr2("localhost", 38801);
    MsgRcvr msgRcvr{};
    hycast::P2pMgr p2pMgr1(servAddr1, 1, nullptr, 60, msgRcvr);
    hycast::P2pMgr p2pMgr2(servAddr1, 1, nullptr, 60, msgRcvr);
    hycast::Completer<void> completer{};
    completer.submit([&p2pMgr1]{ p2pMgr1.run(); });
    completer.submit([&p2pMgr2]{ p2pMgr2.run(); });
    ::sleep(1);
    completer.shutdown();
    completer.get().getResult();
    completer.get().getResult();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
