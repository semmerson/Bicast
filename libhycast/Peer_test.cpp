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

#include "ClientSocket.h"
#include "HycastTypes.h"
#include "InetSockAddr.h"
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
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    PeerTest() {
        // You can do set-up work for each test here.
        prodInfo = hycast::ProdInfo("product", 1, 100000, 1400);
        chunkInfo = hycast::ChunkInfo(2, 3);
        prodIndex = hycast::ProdIndex(2);
        (void)memset(data, 0xbd, sizeof(data));
        actualChunk = hycast::ActualChunk(chunkInfo, data, sizeof(data));
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

    void runTestServer(hycast::ServerSocket serverSock)
    {
        hycast::Socket sock{serverSock.accept()};
        hycast::Peer peer{sock};

        ASSERT_EQ(hycast::MSGTYPE_PROD_NOTICE, peer.getMsgType());
        hycast::ProdInfo prodInfo2{peer.getProdNotice()};
        EXPECT_TRUE(prodInfo.equals(prodInfo2));

        ASSERT_EQ(hycast::MSGTYPE_CHUNK_NOTICE, peer.getMsgType());
        hycast::ChunkInfo chunkInfo2{peer.getChunkNotice()};
        EXPECT_TRUE(chunkInfo.equals(chunkInfo2));

        ASSERT_EQ(hycast::MSGTYPE_PROD_REQUEST, peer.getMsgType());
        hycast::ProdIndex prodIndex2{peer.getProdRequest()};
        EXPECT_TRUE(prodIndex.equals(prodIndex2));

        ASSERT_EQ(hycast::MSGTYPE_CHUNK_REQUEST, peer.getMsgType());
        EXPECT_TRUE(chunkInfo.equals(peer.getChunkRequest()));

        ASSERT_EQ(hycast::MSGTYPE_CHUNK, peer.getMsgType());
        hycast::LatentChunk latentChunk{peer.getChunk()};
        ASSERT_EQ(sizeof(data), latentChunk.getSize());
        char data2[sizeof(data)];
        latentChunk.drainData(data2);
        EXPECT_EQ(0, memcmp(data, data2, sizeof(data)));
    }

    void runTestClient()
    {
        hycast::ClientSocket sock(serverSockAddr, hycast::Peer::getNumStreams());
        hycast::Peer peer{sock};
        peer.sendNotice(prodInfo);
        peer.sendNotice(chunkInfo);
        peer.sendRequest(prodIndex);
        peer.sendRequest(chunkInfo);
        peer.sendData(actualChunk);
    }

    void runDiscardingServer(hycast::ServerSocket serverSock)
    {
        // Just read and discard the incoming objects
        hycast::Socket sock(serverSock.accept());
        hycast::Peer peer{sock};
        for (;;) {
            uint32_t size = sock.getSize();
            if (size == 0)
                break;
            alignas(alignof(max_align_t)) char buf[size];
            sock.recv(buf, size);
        }
    }

    void runPerfClient()
    {
        hycast::ClientSocket sock(serverSockAddr, hycast::Peer::getNumStreams());
        hycast::Peer peer(sock);
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

    void startTestServer()
    {
        // Server socket must exist before client connects
        hycast::ServerSocket sock(serverSockAddr, hycast::Peer::getNumStreams());
        serverThread = std::thread(&PeerTest::runTestServer, this, sock);
    }

    void startTestClient()
    {
        clientThread = std::thread(&PeerTest::runTestClient, this);
    }

    void startDiscardingServer()
    {
        // Server socket must exist before client connects
        hycast::ServerSocket sock(serverSockAddr, hycast::Peer::getNumStreams());
        serverThread = std::thread(&PeerTest::runDiscardingServer, this, sock);
    }

    void startPerfClient()
    {
        clientThread = std::thread(&PeerTest::runPerfClient, this);
    }

    void waitClient()
    {
        clientThread.join();
    }

    void waitServer()
    {
        serverThread.join();
    }

    // Objects declared here can be used by all tests in the test case for Peer.
    std::thread         serverThread;
    std::thread         clientThread;
    hycast::ProdInfo    prodInfo;
    hycast::ChunkInfo   chunkInfo;
    hycast::ProdIndex   prodIndex;
    char                data[2000];
    hycast::ActualChunk actualChunk;
};

// Tests exchange
TEST_F(PeerTest, Exchange) {
    startTestServer();
    startTestClient();
    waitClient();
    waitServer();
}

// Tests performance
TEST_F(PeerTest, Performance) {
    startDiscardingServer();
    startPerfClient();
    waitClient();
    waitServer();
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
