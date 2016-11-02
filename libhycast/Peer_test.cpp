/**
 * This file tests the class `Peer`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Peer_test.cpp
 * @author: Steven R. Emmerson
 */

#include "Peer.h"

#include "ChunkInfo.h"
#include "ClientSocket.h"
#include "HycastTypes.h"
#include "InetSockAddr.h"
#include "PeerMgr.h"
#include "ProdInfo.h"
#include "ServerSocket.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <ctime>
#include <functional>
#include <gtest/gtest.h>
#include <iostream>
#include <mutex>
#include <ratio>
#include <thread>

namespace {

static hycast::InetSockAddr serverSockAddr;

// The fixture for testing class Peer.
class PeerTest : public ::testing::Test {
friend class TestPeerMgr;
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    PeerTest() {
        prodInfo = hycast::ProdInfo("product", 1, 100000, 1400);
        chunkInfo = hycast::ChunkInfo(2, 3);
        prodIndex = hycast::ProdIndex(2);
        (void)memset(data, 0xbd, sizeof(data));
    }

    virtual ~PeerTest() {
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
        PeerTest* peerTest;
    public:
        TestPeerMgr(PeerTest& peerTest)
            : peerTest{&peerTest} {}
        void recvNotice(const hycast::ProdInfo& info) {
            EXPECT_TRUE(peerTest->prodInfo.equals(info));
        }
        void recvNotice(const hycast::ChunkInfo& info) {
            EXPECT_TRUE(peerTest->chunkInfo.equals(info));
        }
        void recvRequest(const hycast::ProdIndex& index) {
            EXPECT_TRUE(peerTest->prodIndex.equals(index));
        }
        void recvRequest(const hycast::ChunkInfo& info) {
            EXPECT_TRUE(peerTest->chunkInfo.equals(info));
        }
        void recvData(hycast::LatentChunk chunk) {
            ASSERT_EQ(sizeof(peerTest->data), chunk.getSize());
            char data2[sizeof(peerTest->data)];
            chunk.drainData(data2);
            EXPECT_EQ(0, memcmp(peerTest->data, data2, sizeof(peerTest->data)));
        }
    };

    void runTestReceiver(hycast::ServerSocket serverSock)
    {
        hycast::Socket sock{serverSock.accept()};
        TestPeerMgr peerMgr{*this};
        hycast::Peer peer{peerMgr, sock};
        peer.runReceiver();
    }

    void runTestSender()
    {
        hycast::ClientSocket sock(serverSockAddr, hycast::Peer::getNumStreams());
        TestPeerMgr peerMgr{*this};
        hycast::Peer peer(peerMgr, sock);
        peer.sendNotice(prodInfo);
        peer.sendNotice(chunkInfo);
        peer.sendRequest(prodIndex);
        peer.sendRequest(chunkInfo);
        hycast::ActualChunk actualChunk(chunkInfo, data, sizeof(data));
        peer.sendData(actualChunk);
    }

    void startTestReceiver()
    {
        // Server socket must exist before client connects
        hycast::ServerSocket sock(serverSockAddr, hycast::Peer::getNumStreams());
        receiverThread = std::thread(&PeerTest::runTestReceiver, this, sock);
    }

    void startTestSender()
    {
        senderThread = std::thread(&PeerTest::runTestSender, this);
    }

    void runPerfReceiver(hycast::ServerSocket serverSock)
    {
        hycast::Socket sock{serverSock.accept()};
        class PerfPeerMgr final : public hycast::PeerMgr {
        public:
            void recvNotice(const hycast::ProdInfo& info) {}
            void recvNotice(const hycast::ChunkInfo& info) {}
            void recvRequest(const hycast::ProdIndex& index) {}
            void recvRequest(const hycast::ChunkInfo& info) {}
            void recvData(hycast::LatentChunk chunk) {
                chunk.discard();
            }
        } peerMgr{};
        hycast::Peer peer{peerMgr, sock};
        peer.runReceiver();
    }

    void runPerfSender()
    {
        hycast::ClientSocket sock(serverSockAddr, hycast::Peer::getNumStreams());
        TestPeerMgr peerMgr{*this};
        hycast::Peer peer(peerMgr, sock);
        const size_t dataSize = 1000000;
        hycast::ChunkInfo chunkInfo(2, 3);
        for (hycast::ChunkSize chunkSize = hycast::chunkSizeMax - 8;
                chunkSize > 4000; chunkSize /= 2) {
            char data[chunkSize];
            std::chrono::high_resolution_clock::time_point start =
                    std::chrono::high_resolution_clock::now();
            size_t remaining = dataSize;
            while (remaining > 0) {
                size_t nbytes = chunkSize < remaining ? chunkSize : remaining;
                hycast::ActualChunk chunk(chunkInfo, data, nbytes);
                peer.sendData(chunk);
                remaining -= nbytes;
            }
            std::chrono::high_resolution_clock::time_point stop =
                    std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> time_span =
                    std::chrono::duration_cast<std::chrono::duration<double>>
                    (stop - start);
            std::cerr << "Chunk size=" + std::to_string(chunkSize) +
                    " bytes, duration=" + std::to_string(time_span.count()) +
                    " s, byte rate=" + std::to_string(dataSize/time_span.count()) +
                    " Hz" << std::endl;
        }
    }

    void startPerfReceiver()
    {
        // Server socket must exist before client connects
        hycast::ServerSocket sock(serverSockAddr, hycast::Peer::getNumStreams());
        receiverThread = std::thread(&PeerTest::runPerfReceiver, this, sock);
    }

    void startPerfSender()
    {
        senderThread = std::thread(&PeerTest::runPerfSender, this);
    }

    void waitReceiver()
    {
        receiverThread.join();
    }

    void waitSender()
    {
        senderThread.join();
    }

    // Objects declared here can be used by all tests in the test case for Peer.
    std::thread       senderThread;
    std::thread       receiverThread;
    hycast::ProdInfo  prodInfo;
    hycast::ChunkInfo chunkInfo;
    hycast::ProdIndex prodIndex;
    char              data[2000];
};

// Tests default construction
TEST_F(PeerTest, DefaultConstruction) {
    hycast::Peer peer{};
}

// Tests transmission
TEST_F(PeerTest, Transmission) {
    startTestReceiver();
    startTestSender();
    waitSender();
    waitReceiver();
}

// Tests performance
TEST_F(PeerTest, Performance) {
    startPerfReceiver();
    startPerfSender();
    waitSender();
    waitReceiver();
}

}  // namespace

int main(int argc, char **argv) {
    const char* serverIpAddrStr = "127.0.0.1";
    if (argc > 1)
        serverIpAddrStr = argv[1];
    serverSockAddr = hycast::InetSockAddr{serverIpAddrStr, 38800};
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
