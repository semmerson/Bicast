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

#include "Peer.h"
#include "PeerConnection.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace {

class ClientPeer final : public hycast::Peer {
    std::mutex              mutex;
    std::condition_variable cond;
    hycast::PeerConnection  conn;
    hycast::ProdInfo        prodInfo;
public:
    ClientPeer(hycast::Socket& sock)
        : mutex(),
          cond(),
          conn(sock) {}

    void sendProdInfo(hycast::ProdInfo& info) {
        conn.sendProdInfo(info);
    }
    void recvProdInfo(std::shared_ptr<hycast::ProdInfo> info) {
        EXPECT_TRUE(info->equals(prodInfo));
    }

    void sendChunkInfo(std::shared_ptr<hycast::ChunkInfo>& info) =0;
    void recvChunkInfo(std::shared_ptr<hycast::ChunkInfo>& info) =0;

    void sendProdRequest(hycast::ProdIndex& index) =0;
    void recvProdRequest(hycast::ProdIndex& index) =0;

    void sendChunkRequest(hycast::ChunkInfo& info) =0;
    void recvChunkRequest(hycast::ChunkInfo& info) =0;

    void sendChunk(hycast::ActualChunk& chunk) =0;
    void recvChunk(hycast::LatentChunk& chunk) =0;
};

static const unsigned             numStreams = 5;
static const hycast::InetSockAddr serverSockAddr("127.0.0.1", 38800);

void runServer(hycast::ServerSocket serverSock)
{
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

void runClient()
{
    hycast::ClientSocket sock(serverSockAddr, numStreams);

    hycast::Channel<hycast::ProdInfo> prodInfoChannel(sock, 0, 0);
    EXPECT_EQ(sock, prodInfoChannel.getSocket());
    hycast::ProdInfo prodInfo1("product", 1, 2, 3);
    prodInfoChannel.send(prodInfo1);
    EXPECT_EQ(0, prodInfoChannel.getStreamId());
    std::shared_ptr<hycast::ProdInfo> prodInfoPtr(prodInfoChannel.recv());
    EXPECT_TRUE(prodInfo1.equals(*prodInfoPtr.get()));

    hycast::Channel<hycast::ChunkInfo> chunkInfoChannel(sock, 1, 0);
    EXPECT_EQ(sock, chunkInfoChannel.getSocket());
    hycast::ChunkInfo chunkInfo1(4, 5);
    chunkInfoChannel.send(chunkInfo1);
    EXPECT_EQ(1, chunkInfoChannel.getStreamId());
    std::shared_ptr<hycast::ChunkInfo> chunkInfoPtr(chunkInfoChannel.recv());
    EXPECT_TRUE(chunkInfo1.equals(*chunkInfoPtr.get()));
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

    // Objects declared here can be used by all tests in the test case for PeerConnection.
    std::thread             clientThread;
    std::thread             serverThread;
};

// Tests Construction
TEST_F(PeerConnectionTest, Construction) {
    hycast::Socket         sock;
    hycast::PeerConnection conn{sock};
}

// Tests transmission
TEST_F(PeerConnectionTest, Transmission) {
    startServer();
    startClient();
    ASSERT_TRUE(false);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
