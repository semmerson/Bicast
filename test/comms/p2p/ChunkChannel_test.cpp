/**
 * This file tests the data-chunk transmission of class `Channel`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ChunkChannel_test.cpp
 * @author: Steven R. Emmerson
 */

#include "Chunk.h"
#include "InetSockAddr.h"
#include "ProdInfo.h"
#include "SctpSock.h"

#include <cstring>
#include <gtest/gtest.h>
#include <p2p/Channel.h>
#include <thread>

namespace {

static const unsigned             numStreams = 1;
static const hycast::InetSockAddr serverSockAddr("127.0.0.1", 38800);

void runServer(hycast::SrvrSctpSock serverSock)
{
    hycast::SctpSock connSock(serverSock.accept());
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
    hycast::SctpSock sock(serverSockAddr, numStreams);

    hycast::Channel<hycast::ActualChunk,hycast::LatentChunk> chunkChannel(sock, 0, 0);
    uint8_t data1[hycast::ChunkInfo::getCanonSize()];
    (void)memset(data1, 0xbd, sizeof(data1));
    hycast::ProdInfo  prodInfo("product", 0, sizeof(data1));
    hycast::ChunkInfo chunkInfo(prodInfo, 0);
    hycast::ActualChunk actualChunk(chunkInfo, data1);
    chunkChannel.send(actualChunk);
    char data2[sizeof(data1)];
    ASSERT_EQ(0, sock.getStreamId());
    hycast::LatentChunk latentChunk = chunkChannel.recv();
    const size_t nbytes = latentChunk.drainData(data2, sizeof(data2));
    EXPECT_EQ(sizeof(data2), nbytes);
    EXPECT_EQ(0, memcmp(data1, data2, sizeof(data1)));
}

// The fixture for testing class Chunk.
class ChunkChannelTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    ChunkChannelTest() {
        // You can do set-up work for each test here.
    }

    virtual ~ChunkChannelTest() {
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

    // Objects declared here can be used by all tests in the test case for Chunk.

    void startServer()
    {
        // Server socket must exist before client connects
        hycast::SrvrSctpSock sock(serverSockAddr, numStreams);
        sock.listen();
        serverThread = std::thread(runServer, sock);
    }

    void startClient()
    {
        clientThread = std::thread(runClient);
    }

    void stopClient()
    {
        clientThread.join();
    }

    void stopServer()
    {
        serverThread.join();
    }

    // Objects declared here can be used by all tests in the test case for Channel.
    std::thread clientThread;
    std::thread serverThread;
};

// Tests default construction
TEST_F(ChunkChannelTest, DefaultConstruction) {
    hycast::Channel<hycast::ActualChunk,hycast::LatentChunk> chunkChannel{};
}

// Tests end-to-end Chunk transmission
TEST_F(ChunkChannelTest, EndToEnd) {
    startServer();
    startClient();
    stopClient();
    stopServer();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
