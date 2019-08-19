/**
 * This file tests class `RemotePeer`.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: RemotePeet_test.cpp
 * Created On: May 28, 2019
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "SockAddr.h"
#include "RemotePeer.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `RemotePeer`
class RemotePeerTest : public ::testing::Test
{
protected:
    hycast::SockAddr        srvrAddr;
    std::mutex              mutex;
    std::condition_variable cond;
    bool                    srvrReady;
    hycast::ChunkId         chunkId;
    char                    memData[1000] = {0};
    hycast::MemChunk        memChunk;

    // You can remove any or all of the following functions if its body
    // is empty.

    RemotePeerTest()
        : srvrAddr{"localhost:38800"}
        , mutex{}
        , cond{}
        , srvrReady{false}
        , chunkId{1}
        , memChunk(chunkId, sizeof(memData), memData)
    {
        // You can do set-up work for each test here.
    }

    virtual ~RemotePeerTest()
    {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp()
    {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    virtual void TearDown()
    {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    // Objects declared here can be used by all tests in the test case for Error.

public:
    void runServer(hycast::SrvrSock& srvrSock)
    {
        {
            std::lock_guard<decltype(mutex)> lock{mutex};
            srvrSock.listen(1);
            srvrReady = true;
            cond.notify_one();
        }

        hycast::PortPool   portPool(38801, 38802);
        hycast::Socket     sock{srvrSock.accept()};
        hycast::RemotePeer rmtPeer(sock, portPool);
        hycast::ChunkId    id = rmtPeer.getNotice();
        EXPECT_EQ(chunkId, id);

        rmtPeer.request(id);

        hycast::WireChunk wireChunk = rmtPeer.getChunk();
        EXPECT_EQ(chunkId, wireChunk.getId());
        hycast::ChunkSize n = wireChunk.getSize();
        EXPECT_EQ(memChunk.getSize(), n);

        char wireData[n];
        wireChunk.read(wireData);
        EXPECT_EQ(0, ::memcmp(memData, wireData, n));
    }
};

// Tests default construction
TEST_F(RemotePeerTest, DefaultConstruction)
{
    hycast::RemotePeer rmtPeer();
}

// Tests a three `Wire` remote peer
TEST_F(RemotePeerTest, ThreeWireRemotePeer)
{
    hycast::SrvrSock srvrSock(srvrAddr);
    std::thread      srvrThread(&RemotePeerTest::runServer, this,
            std::ref(srvrSock));

    {
        /*
         * Necessary because `ClntSock` constructor throws if `::connect()`
         * called before `::listen()`
         */
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (!srvrReady)
            cond.wait(lock);
    }

    hycast::RemotePeer rmtPeer(srvrAddr);

    rmtPeer.notify(chunkId);

    hycast::ChunkId id = rmtPeer.getRequest();
    EXPECT_EQ(chunkId, id);

    rmtPeer.send(memChunk);

    srvrThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
