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

#include "ChunkInfo.h"
#include "HycastTypes.h"
#include "InetSockAddr.h"
#include "ProdInfo.h"
#include "SctpSock.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <ctime>
#include <functional>
#include <gtest/gtest.h>
#include <p2p/Peer.h>
#include <p2p/PeerMsgRcvr.h>
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

    PeerTest()
        : data{new char[hycast::ChunkInfo::getCanonSize()]}
    {}

    virtual ~PeerTest()
    {
        delete[] data;
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

    class TestMsgRcvr final : public hycast::PeerMsgRcvr {
        PeerTest* peerTest;
    public:
        TestMsgRcvr(PeerTest& peerTest)
            : peerTest{&peerTest} {}
        void recvNotice(const hycast::ProdInfo& info, const hycast::Peer& peer) {
            EXPECT_TRUE(peerTest->prodInfo == info);
        }
        void recvNotice(const hycast::ChunkInfo& info, const hycast::Peer& peer) {
            EXPECT_TRUE(peerTest->chunkInfo == info);
        }
        void recvRequest(const hycast::ProdIndex& index, const hycast::Peer& peer) {
            EXPECT_TRUE(peerTest->prodIndex == index);
        }
        void recvRequest(const hycast::ChunkInfo& info, const hycast::Peer& peer) {
            EXPECT_TRUE(peerTest->chunkInfo == info);
        }
        void recvData(hycast::LatentChunk chunk, const hycast::Peer& peer) {
            hycast::ChunkSize expectedSize =
                    peerTest->prodInfo.getChunkSize(chunk.getIndex());
            char data2[expectedSize];
            const size_t actualSize = chunk.drainData(data2, sizeof(data2));
            ASSERT_EQ(expectedSize, actualSize);
            EXPECT_EQ(0, memcmp(peerTest->data, data2, actualSize));
        }
    };

    void runTestReceiver(const hycast::SrvrSctpSock& serverSock)
    {
        hycast::SctpSock sock{serverSock.accept()};
        TestMsgRcvr msgRcvr{*this};
        hycast::Peer peer{msgRcvr, sock};
        peer.runReceiver();
    }

    void runTestSender()
    {
        hycast::SctpSock sock(hycast::Peer::getNumStreams());
        sock.connect(serverSockAddr);
        TestMsgRcvr msgRcvr{*this};
        hycast::Peer peer(msgRcvr, sock);
        peer.sendNotice(prodInfo);
        peer.sendNotice(chunkInfo);
        peer.sendRequest(prodIndex);
        peer.sendRequest(chunkInfo);
        hycast::ActualChunk actualChunk(chunkInfo, data);
        peer.sendData(actualChunk);
    }

    void startTestReceiver()
    {
        // Server socket must exist before client connects
        hycast::SrvrSctpSock sock(serverSockAddr, hycast::Peer::getNumStreams());
        sock.listen();
        receiverThread = std::thread([=]{ this->runTestReceiver(sock); });
    }

    void startTestSender()
    {
        senderThread = std::thread(&PeerTest::runTestSender, this);
    }

    void runPerfReceiver(hycast::SrvrSctpSock serverSock)
    {
        hycast::SctpSock sock{serverSock.accept()};
        class PerfMsgRcvr final : public hycast::PeerMsgRcvr {
        public:
            void recvNotice(const hycast::ProdInfo& info) {}
            void recvNotice(const hycast::ProdInfo& info, const hycast::Peer& peer) {}
            void recvNotice(const hycast::ChunkInfo& info, const hycast::Peer& peer) {}
            void recvRequest(const hycast::ProdIndex& index, const hycast::Peer& peer) {}
            void recvRequest(const hycast::ChunkInfo& info, const hycast::Peer& peer) {}
            void recvData(hycast::LatentChunk chunk) {}
            void recvData(hycast::LatentChunk chunk, const hycast::Peer& peer) {
                chunk.discard();
            }
        };
        PerfMsgRcvr msgRcvr{};
        hycast::Peer peer{msgRcvr, sock};
        peer.runReceiver();
    }

    void runPerfSender()
    {
        hycast::SctpSock sock(hycast::Peer::getNumStreams());
        sock.connect(serverSockAddr);
        TestMsgRcvr msgRcvr{*this};
        hycast::Peer peer(msgRcvr, sock);
        const size_t dataSize = 1000000;
        hycast::ProdInfo prodInfo("product", 0, dataSize);
        for (hycast::ChunkSize chunkSize = hycast::chunkSizeMax - 8;
                chunkSize > 4000; chunkSize /= 2) {
            char              data[chunkSize];
            std::chrono::high_resolution_clock::time_point start =
                    std::chrono::high_resolution_clock::now();
            size_t remaining = dataSize;
            for (hycast::ChunkIndex chunkIndex = 0;
                    chunkIndex < prodInfo.getNumChunks(); ++chunkIndex) {
                hycast::ChunkInfo chunkInfo(prodInfo, chunkIndex);
                hycast::ActualChunk chunk(chunkInfo, data+chunkInfo.getOffset());
                peer.sendData(chunk);
                remaining -= chunk.getSize();
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
        hycast::SrvrSctpSock sock(serverSockAddr, hycast::Peer::getNumStreams());
        sock.listen();
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
    hycast::ProdInfo  prodInfo{"product", 1, 100000};
    hycast::ChunkInfo chunkInfo{prodInfo, 3};
    hycast::ProdIndex prodIndex{1};
    char*             data;
};

// Tests default construction
TEST_F(PeerTest, DefaultConstruction) {
    hycast::Peer peer{};
}

// Tests to_string
TEST_F(PeerTest, ToString) {
    EXPECT_STREQ("{addr=:0, version=0, sock={sd=-1, numStreams=0}}",
            hycast::Peer().to_string().data());
}

// Tests transmission
TEST_F(PeerTest, Transmission) {
    startTestReceiver();
    startTestSender();
    waitSender();
    waitReceiver();
}

#if 0
// Tests performance
TEST_F(PeerTest, Performance) {
    startPerfReceiver();
    startPerfSender();
    waitSender();
    waitReceiver();
}
#endif

}  // namespace

int main(int argc, char **argv) {
    const char* serverIpAddrStr = "127.0.0.1";
    if (argc > 1)
        serverIpAddrStr = argv[1];
    serverSockAddr = hycast::InetSockAddr{serverIpAddrStr, 38800};
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
