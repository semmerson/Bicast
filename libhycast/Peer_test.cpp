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
#include "PeerMgr.h"
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

static const unsigned version = 0;

class AsyncClientPeerMgr final : public hycast::PeerMgr {
    hycast::Peer       peer;
public:
    AsyncClientPeerMgr(
            hycast::Socket& sock,
            const unsigned  version)
        : peer(*this, sock, version) {}

    void sendNotice(const hycast::ProdInfo& info) {
        peer.sendNotice(info);
    }
    void recvNotice(const hycast::ProdInfo& info) {
    }

    void sendNotice(const hycast::ChunkInfo& info) {
        peer.sendNotice(info);
    }
    void recvNotice(const hycast::ChunkInfo& info) {
    }

    void sendRequest(const hycast::ProdIndex& index) {
        peer.sendRequest(index);
    }
    void recvRequest(const hycast::ProdIndex& index) {
    }

    void sendRequest(const hycast::ChunkInfo& info) {
        peer.sendRequest(info);
    }
    void recvRequest(const hycast::ChunkInfo& info) {
    }

    void sendData(const hycast::ActualChunk& chunk) {
        peer.sendData(chunk);
    }
    void recvData(hycast::LatentChunk chunk) {
    }

    void recvEof() {
    }

    void recvException(const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
};

class SyncClientPeerMgr final : public hycast::PeerMgr {
    std::mutex                   mutex;
    std::condition_variable_any  cond;
    bool                         received;
    hycast::Peer                 peer;
    hycast::ProdInfo             prodInfo;
    hycast::ChunkInfo            chunkInfo;
    hycast::ProdIndex            prodIndex;
    hycast::ActualChunk          actualChunk;
    std::shared_ptr<char>        data;
public:
    SyncClientPeerMgr(
            hycast::Socket& sock,
            const unsigned  version)
        : mutex(),
          cond(),
          received(false),
          peer(*this, sock, version),
          data(nullptr) {}

    void sendNotice(const hycast::ProdInfo& info) {
        std::lock_guard<std::mutex> guard(mutex);
        received = false;
        peer.sendNotice(info);
        while (!received)
            cond.wait(mutex);
    }
    void recvNotice(const hycast::ProdInfo& info) {
        std::lock_guard<std::mutex> guard(mutex);
        prodInfo = info;
        received = true;
        cond.notify_one();
    }
    hycast::ProdInfo getProdInfo() {
        return prodInfo;
    }

    void sendNotice(const hycast::ChunkInfo& info) {
        std::lock_guard<std::mutex> guard(mutex);
        received = false;
        peer.sendNotice(info);
        while (!received)
            cond.wait(mutex);
    }
    void recvNotice(const hycast::ChunkInfo& info) {
        std::lock_guard<std::mutex> guard(mutex);
        chunkInfo = info;
        received = true;
        cond.notify_one();
    }
    hycast::ChunkInfo getChunkInfo() {
        return chunkInfo;
    }

    void sendRequest(const hycast::ProdIndex& index) {
        std::lock_guard<std::mutex> guard(mutex);
        received = false;
        peer.sendRequest(index);
        while (!received)
            cond.wait(mutex);
    }
    void recvRequest(const hycast::ProdIndex& index) {
        std::lock_guard<std::mutex> guard(mutex);
        prodIndex = index;
        received = true;
        cond.notify_one();
    }
    hycast::ProdIndex getProdIndex() {
        return prodIndex;
    }

    void sendRequest(const hycast::ChunkInfo& info) {
        std::lock_guard<std::mutex> guard(mutex);
        received = false;
        peer.sendRequest(info);
        while (!received)
            cond.wait(mutex);
    }
    void recvRequest(const hycast::ChunkInfo& info) {
        std::lock_guard<std::mutex> guard(mutex);
        chunkInfo = info;
        received = true;
        cond.notify_one();
    }

    void sendData(const hycast::ActualChunk& chunk) {
        std::lock_guard<std::mutex> guard(mutex);
        received = false;
        peer.sendData(chunk);
        while (!received)
            cond.wait(mutex);
    }
    void recvData(hycast::LatentChunk chunk) {
        std::lock_guard<std::mutex> guard(mutex);
        data = std::shared_ptr<char>(new char[chunk.getSize()]);
        chunk.drainData(data.get());
        actualChunk = hycast::ActualChunk(chunk.getInfo(), data.get(),
                chunk.getSize());
        received = true;
        cond.notify_one();
    }
    hycast::ActualChunk getActualChunk() {
        return actualChunk;
    }

    void recvEof() {
    }

    void recvException(const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
};

static const unsigned       numStreams = 5;
static hycast::InetSockAddr serverSockAddr;

void runAsyncClient()
{
    hycast::ClientSocket sock(serverSockAddr, numStreams);
    AsyncClientPeerMgr peerMgr(sock, version);
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
            peerMgr.sendData(chunk);
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

void runSyncClient()
{
    hycast::ClientSocket sock(serverSockAddr, numStreams);
    SyncClientPeerMgr peerMgr(sock, version);

    hycast::ProdInfo prodInfo("product", 1, 100000, 1400);
    peerMgr.sendNotice(prodInfo);
    EXPECT_TRUE(prodInfo.equals(peerMgr.getProdInfo()));

    hycast::ChunkInfo chunkInfo(2, 3);
    peerMgr.sendNotice(chunkInfo);
    EXPECT_TRUE(chunkInfo.equals(peerMgr.getChunkInfo()));

    hycast::ProdIndex prodIndex(2);
    peerMgr.sendRequest(prodIndex);
    EXPECT_TRUE(prodIndex.equals(peerMgr.getProdIndex()));

    peerMgr.sendRequest(chunkInfo);
    EXPECT_TRUE(chunkInfo.equals(peerMgr.getChunkInfo()));

    char actualData[2000];
    (void)memset(actualData, 0xbd, sizeof(actualData));
    hycast::ActualChunk actualChunk1(chunkInfo, actualData, sizeof(actualData));
    peerMgr.sendData(actualChunk1);
    hycast::ActualChunk actualChunk2 = peerMgr.getActualChunk();
    ASSERT_EQ(sizeof(actualData), actualChunk2.getSize());
    EXPECT_EQ(0, memcmp(actualChunk1.getData(), actualChunk2.getData(),
            sizeof(actualData)));
}

void runAsyncServer(hycast::ServerSocket serverSock)
{
    // Just read and discard the incoming objects
    hycast::Socket sock(serverSock.accept());
    for (;;) {
        uint32_t size = sock.getSize();
        if (size == 0)
            break;
        alignas(alignof(max_align_t)) char buf[size];
        sock.recv(buf, size);
    }
}

void runSyncServer(hycast::ServerSocket serverSock)
{
    // Just echo the incoming objects back to the client at the socket level
    hycast::Socket sock(serverSock.accept());
    for (;;) {
        uint32_t size = sock.getSize();
        if (size == 0)
            break;
        unsigned streamId = sock.getStreamId();
        alignas(alignof(max_align_t)) char buf[size];
        sock.recv(buf, size);
        sock.send(streamId, buf, size);
    }
}

// The fixture for testing class Peer.
class PeerTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    PeerTest() {
        // You can do set-up work for each test here.
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

    void startSyncServer()
    {
        // Server socket must exist before client connects
        hycast::ServerSocket sock(serverSockAddr, numStreams);
        serverThread = std::thread(runSyncServer, sock);
    }

    void startAsyncServer()
    {
        // Server socket must exist before client connects
        hycast::ServerSocket sock(serverSockAddr, numStreams);
        serverThread = std::thread(runAsyncServer, sock);
    }

    void startSyncClient()
    {
        clientThread = std::thread(runSyncClient);
    }

    void startAsyncClient()
    {
        clientThread = std::thread(runAsyncClient);
    }

    void waitServer()
    {
        serverThread.join();
    }

    void waitClient()
    {
        clientThread.join();
    }

    // Objects declared here can be used by all tests in the test case for Peer.
    std::thread clientThread;
    std::thread serverThread;
};

// Tests transmission
TEST_F(PeerTest, Transmission) {
    startSyncServer();
    startSyncClient();
    waitClient();
    waitServer();
}

// Tests performance
TEST_F(PeerTest, Performance) {
    startAsyncServer();
    startAsyncClient();
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
