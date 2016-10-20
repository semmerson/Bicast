/**
 * This file tests the class `PeerConnection`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnection_test.cpp
 * @author: Steven R. Emmerson
 */

#include "ClientSocket.h"
#include "InetSockAddr.h"
#include "Peer.h"
#include "PeerConnection.h"
#include "ServerSocket.h"

#include <condition_variable>
#include <functional>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace {

static const unsigned version = 0;

class ClientPeer final : public hycast::Peer {
    std::mutex                   mutex;
    std::condition_variable_any  cond;
    bool                         compared;
    hycast::PeerConnection       conn;
    hycast::ProdInfo             prodInfo;
    hycast::ChunkInfo            chunkInfo;
    hycast::ProdIndex            prodIndex;
    hycast::ActualChunk          actualChunk;
public:
    ClientPeer(
            hycast::Socket& sock,
            const unsigned  version)
        : mutex(),
          cond(),
          compared(true),
          conn(*this, sock, version) {}

    void sendNotice(const hycast::ProdInfo& info) {
        std::lock_guard<std::mutex> guard(mutex);
        prodInfo = info;
        compared = false;
        conn.sendNotice(info);
        while (!compared)
            cond.wait(mutex);
    }
    void recvNotice(const hycast::ProdInfo& info) {
        std::lock_guard<std::mutex> guard(mutex);
        EXPECT_TRUE(info.equals(prodInfo));
        compared = true;
        cond.notify_one();
    }

    void sendNotice(const hycast::ChunkInfo& info) {
        std::lock_guard<std::mutex> guard(mutex);
        chunkInfo = info;
        compared = false;
        conn.sendNotice(info);
        while (!compared)
            cond.wait(mutex);
    }
    void recvNotice(const hycast::ChunkInfo& info) {
        std::lock_guard<std::mutex> guard(mutex);
        EXPECT_TRUE(info.equals(chunkInfo));
        compared = true;
        cond.notify_one();
    }

    void sendRequest(const hycast::ProdIndex& index) {
        std::lock_guard<std::mutex> guard(mutex);
        prodIndex = index;
        compared = false;
        conn.sendRequest(index);
        while (!compared)
            cond.wait(mutex);
    }
    void recvRequest(const hycast::ProdIndex& index) {
        std::lock_guard<std::mutex> guard(mutex);
        EXPECT_TRUE(index.equals(prodIndex));
        compared = true;
        cond.notify_one();
    }

    void sendRequest(const hycast::ChunkInfo& info) {
        std::lock_guard<std::mutex> guard(mutex);
        chunkInfo = info;
        compared = false;
        conn.sendRequest(info);
        while (!compared)
            cond.wait(mutex);
    }
    void recvRequest(const hycast::ChunkInfo& info) {
        std::lock_guard<std::mutex> guard(mutex);
        EXPECT_TRUE(info.equals(chunkInfo));
        compared = true;
        cond.notify_one();
    }

    void sendData(const hycast::ActualChunk& chunk) {
        std::lock_guard<std::mutex> guard(mutex);
        actualChunk = chunk;
        compared = false;
        conn.sendData(chunk);
        while (!compared)
            cond.wait(mutex);
    }
    void recvData(hycast::LatentChunk chunk) {
        std::lock_guard<std::mutex> guard(mutex);
        char data[chunk.getSize()];
        chunk.drainData(data);
        EXPECT_EQ(0, memcmp(data, actualChunk.getData(), sizeof(data)));
        compared = true;
        cond.notify_one();
    }

    void recvEof() {
    }

    void recvException(const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
};

static const unsigned             numStreams = 5;
static const hycast::InetSockAddr serverSockAddr("127.0.0.1", 38800);

void runClient()
{
    hycast::ClientSocket sock(serverSockAddr, numStreams);
    ClientPeer peer(sock, version);

    hycast::ProdInfo prodInfo("product", 1, 100000, 1400);
    peer.sendNotice(prodInfo);

    hycast::ChunkInfo chunkInfo(2, 3);
    peer.sendNotice(chunkInfo);

    hycast::ProdIndex prodIndex(2);
    peer.sendRequest(prodIndex);

    peer.sendRequest(chunkInfo);

    char data[2000];
    (void)memset(data, 0xbd, sizeof(data));
    hycast::ActualChunk chunk(chunkInfo, data, sizeof(data));
    peer.sendData(chunk);
}

void runServer(hycast::ServerSocket serverSock)
{
    // Just echo the incoming objects back to the client
    hycast::Socket connSock(serverSock.accept());
    for (;;) {
        uint32_t size = connSock.getSize();
        if (size == 0)
            break;
        unsigned streamId = connSock.getStreamId();
        alignas(alignof(max_align_t)) char buf[size];
        connSock.recv(buf, size);
        connSock.send(streamId, buf, size);
    }
}

// The fixture for testing class PeerConnection.
class PeerConnectionTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    PeerConnectionTest() {
        // You can do set-up work for each test here.
    }

    virtual ~PeerConnectionTest() {
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

    void startServer()
    {
        // Server socket must exist before client connects
        hycast::ServerSocket sock(serverSockAddr, numStreams);
        serverThread = std::thread(runServer, sock);
    }

    void startClient()
    {
        clientThread = std::thread(runClient);
    }

    void waitServer()
    {
        serverThread.join();
    }

    void waitClient()
    {
        clientThread.join();
    }

    // Objects declared here can be used by all tests in the test case for PeerConnection.
    std::thread clientThread;
    std::thread serverThread;
};

// Tests transmission
TEST_F(PeerConnectionTest, Transmission) {
    startServer();
    startClient();
    waitClient();
    waitServer();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
