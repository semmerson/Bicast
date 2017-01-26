/**
 * This file tests the class `ChunkChannel`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ChunkChannel_test.cpp
 * @author: Steven R. Emmerson
 */

#include <comms/ChunkChannel.h>
#include "Chunk.h"
#include "ClntSctpSock.h"
#include "InetSockAddr.h"
#include "SrvrSctpSock.h"

#include <cstring>
#include <gtest/gtest.h>
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
    hycast::ClntSctpSock sock(serverSockAddr, numStreams);

    hycast::ChunkChannel chunkChannel(sock, 0, 0);
    hycast::ChunkInfo chunkInfo(4, 5);
    uint8_t data1[10000] = {};
    (void)memset(data1, 0xbd, sizeof(data1));
    hycast::ActualChunk actualChunk(chunkInfo, data1, sizeof(data1));
    chunkChannel.send(actualChunk);

    ASSERT_EQ(0, chunkChannel.getStreamId());
    hycast::LatentChunk latentChunk = chunkChannel.recv();
    ASSERT_EQ(sizeof(data1), latentChunk.getSize());
    uint8_t data2[sizeof(data1)];
    latentChunk.drainData(data2);
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
    hycast::ChunkChannel chunkChannel();
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
