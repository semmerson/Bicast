/**
 * This file tests class `PeerSet`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerSet_test.cpp
 * @author: Steven R. Emmerson
 */

#include "ClientSocket.h"
#include "PeerMgr.h"
#include "PeerSet.h"
#include "ProdInfo.h"
#include "ServerSocket.h"
#include "Socket.h"

#include <gtest/gtest.h>
#include <thread>

namespace {

// The fixture for testing class PeerSet.
class PeerSetTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    PeerSetTest() {
        // You can do set-up work for each test here.
    }

    virtual ~PeerSetTest() {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp() {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    virtual void TearDown() {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    class TestPeerMgr final : public hycast::PeerMgr {
        PeerSetTest* peerSetTest;
    public:
        TestPeerMgr(PeerSetTest& peerSetTest)
            : peerSetTest{&peerSetTest} {}
        void recvNotice(const hycast::ProdInfo& info, hycast::Peer& peer) {
            EXPECT_TRUE(peerSetTest->prodInfo == info);
        }
        void recvNotice(const hycast::ChunkInfo& info, hycast::Peer& peer) {
            EXPECT_TRUE(peerSetTest->chunkInfo == info);
        }
        void recvRequest(const hycast::ProdIndex& index, hycast::Peer& peer) {
        }
        void recvRequest(const hycast::ChunkInfo& info, hycast::Peer& peer) {
        }
        void recvData(hycast::LatentChunk chunk, hycast::Peer& peer) {
        }
    };

    void runTestReceiver(hycast::ServerSocket& serverSock)
    {
        hycast::Socket       sock{serverSock.accept()};
        TestPeerMgr          peerMgr{*this};
        hycast::Peer{peerMgr, sock}.runReceiver();
    }

    void runTestSender(const hycast::InetSockAddr& serverSockAddr)
    {
        hycast::ClientSocket sock{serverSockAddr, hycast::Peer::getNumStreams()};
        TestPeerMgr peerMgr{*this};
        hycast::Peer peer(peerMgr, sock);
        hycast::PeerSet peerSet{};
        peerSet.insert(peer);
        peerSet.sendNotice(prodInfo);
        peerSet.sendNotice(chunkInfo);
    }

    // Objects declared here can be used by all tests in the test case for PeerSet.
    hycast::ProdInfo prodInfo{"product", 1, 100000, 32000};
    hycast::ChunkInfo chunkInfo{hycast::ProdIndex(1), 2};
};

// Tests default construction
TEST_F(PeerSetTest, DefaultConstruction) {
    hycast::PeerSet peerSet{};
}

// Tests inserting a peer
TEST_F(PeerSetTest, PeerInsertion) {
    hycast::Peer peer{};
    hycast::PeerSet  peerSet{};
    peerSet.insert(peer);
}

// Tests sending notices
TEST_F(PeerSetTest, SendProdNotice) {
    // Receiver socket must exist before client connects
    hycast::InetSockAddr serverSockAddr{"127.0.0.1", 38800};
    hycast::ServerSocket serverSock{serverSockAddr,
        hycast::Peer::getNumStreams()};
    std::thread          recvThread = std::thread([this, &serverSock](){
            this->runTestReceiver(serverSock);});
    std::thread          sendThread = std::thread([this, &serverSockAddr](){
            this->runTestSender(serverSockAddr);});
    sendThread.join();
    recvThread.join();
}

// Tests incrementing the value of a peer
TEST_F(PeerSetTest, IncrementValue) {
    hycast::Peer     peer{};
    hycast::PeerSet  peerSet{};
    peerSet.insert(peer);
    peerSet.incValue(peer);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
