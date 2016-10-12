/**
 * This file tests the class `Chunk`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Chunk_test.cpp
 * @author: Steven R. Emmerson
 */

#include "Channel.h"
#include "Chunk.h"

#include <gtest/gtest.h>

namespace {

static const unsigned             numStreams = 1;
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

    hycast::ChunkChannel chunkChannel(sock, 0, 0);
    hycast::ChunkInfo chunkInfo(4, 5);
    uint8_t data[10000] = {};
    hycast::ActualChunk(chunkInfo, data, sizeof(data));
    chunkChannel.send(ActualChunk);
    EXPECT_EQ(1, chunkInfoChannel.getStreamId());
    std::shared_ptr<hycast::ChunkInfo> chunkInfoPtr(chunkInfoChannel.recv());
    EXPECT_TRUE(chunkInfo1.equals(*chunkInfoPtr.get()));
}

// The fixture for testing class Chunk.
class ChunkTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    ChunkTest() {
        // You can do set-up work for each test here.
    }

    virtual ~ChunkTest() {
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
};

// Tests end-to-end Chunk transmission
TEST_F(ChunkTest, EndToEnd) {
    EXPECT_EQ(0, 0);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
