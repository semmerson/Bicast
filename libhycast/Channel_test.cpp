/**
 * This file tests the class `Channel`
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Channel_test.cpp
 * @author: Steven R. Emmerson
 */


#include "Channel.h"
#include "ChunkInfo.h"
#include "ClientSocket.h"
#include "ProdInfo.h"
#include "ServerSocket.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <thread>

namespace {

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
    hycast::ProdInfo prodInfo2(prodInfoChannel.recv());
    EXPECT_TRUE(prodInfo1.equals(prodInfo2));

    hycast::Channel<hycast::ChunkInfo> chunkInfoChannel(sock, 1, 0);
    EXPECT_EQ(sock, chunkInfoChannel.getSocket());
    hycast::ChunkInfo chunkInfo1(4, 5);
    chunkInfoChannel.send(chunkInfo1);
    EXPECT_EQ(1, chunkInfoChannel.getStreamId());
    hycast::ChunkInfo chunkInfo2(chunkInfoChannel.recv());
    EXPECT_TRUE(chunkInfo1.equals(chunkInfo2));
}

// The fixture for testing class Channel.
class ChannelTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    ChannelTest() {
        // You can do set-up work for each test here.
    }

    virtual ~ChannelTest() {
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

// Tests end-to-end execution.
TEST_F(ChannelTest, EndToEnd) {
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
