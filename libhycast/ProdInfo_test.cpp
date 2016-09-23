/**
 * This file ...
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdInfo_test.cpp
 * @author: Steven R. Emmerson
 */


#include "ProdInfo.h"
#include "ClientSocket.h"
#include "InetSockAddr.h"
#include "ServerSocket.h"
#include "Socket.h"

#include <arpa/inet.h>
#include <cstring>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <thread>

namespace {

static const int MY_PORT_NUM = 38800;
static const int numStreams = 1;
static const int version = 0;

void runServer(hycast::ServerSocket serverSock)
{
    hycast::Socket sock(serverSock.accept());
    for (;;) {
#if 0
        hycast::ProdInfo info(sock, version);
        EXPECT_STREQ("name", info.getName().data());
        EXPECT_EQ(1, info.getIndex());
        EXPECT_EQ(2, info.getSize());
        EXPECT_EQ(3, info.getChunkSize());
        info.serialize(sock, 0, version);
        break;
#else
        uint32_t size = sock.getSize();
        if (size == 0)
            break;
        unsigned streamId = sock.getStreamId();
        char buf[size];
        sock.recv(buf, size);
        sock.send(streamId, buf, size);
#endif
    }
}

void runClient()
{
    hycast::InetSockAddr sockAddr("127.0.0.1", MY_PORT_NUM);
    hycast::ClientSocket sock(sockAddr, numStreams);
    hycast::ProdInfo info1("name", 1, 2, 3);
    info1.serialize(sock, 0, version);
    hycast::ProdInfo info2(sock, version);
    EXPECT_STREQ(info1.getName().data(), info2.getName().data());
    EXPECT_EQ(info1.getIndex(), info2.getIndex());
    EXPECT_EQ(info1.getSize(), info2.getSize());
    EXPECT_EQ(info1.getChunkSize(), info2.getChunkSize());
}

// The fixture for testing class ProdInfo.
class ProdInfoTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  ProdInfoTest() {
    // You can do set-up work for each test here.
  }

  virtual ~ProdInfoTest() {
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
        // Work done here so that socket is being listened to when client starts
        hycast::InetSockAddr sockAddr("127.0.0.1", MY_PORT_NUM);
        hycast::ServerSocket sock(sockAddr, numStreams);
        recvThread = std::thread(runServer, sock);
    }

  // Objects declared here can be used by all tests in the test case for ProdInfo.
    std::thread recvThread;
    std::thread sendThread;

    void startClient()
    {
        sendThread = std::thread(runClient);
    }

    void stopClient()
    {
        sendThread.join();
    }

    void stopServer()
    {
        recvThread.join();
    }
};

// Tests default construction
TEST_F(ProdInfoTest, DefaultConstruction) {
    hycast::ProdInfo info;
    EXPECT_STREQ("", info.getName().data());
    EXPECT_EQ(0, info.getIndex());
    EXPECT_EQ(0, info.getSize());
    EXPECT_EQ(0, info.getChunkSize());
}

// Tests construction
TEST_F(ProdInfoTest, Construction) {
    hycast::ProdInfo info("name", 1, 2, 3);
    EXPECT_STREQ("name", info.getName().data());
    EXPECT_EQ(1, info.getIndex());
    EXPECT_EQ(2, info.getSize());
    EXPECT_EQ(3, info.getChunkSize());
}

// Tests serialization/deserialization
TEST_F(ProdInfoTest, Serialization) {
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
