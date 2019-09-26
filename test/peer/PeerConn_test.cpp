/**
 * This file tests class `PeerConn`.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: PeerConn_test.cpp
 * Created On: May 28, 2019
 *     Author: Steven R. Emmerson
 */
#include "config.h"
#include "error.h"
#include "PeerConn.h"
#include "SockAddr.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `RemotePeer`
class PeerConnTest : public ::testing::Test
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

    PeerConnTest()
        : srvrAddr{"localhost:38800"}
        , mutex{}
        , cond{}
        , srvrReady{false}
        , chunkId{1}
        , memChunk(chunkId, sizeof(memData), memData)
    {
        // You can do set-up work for each test here.
    }

    virtual ~PeerConnTest()
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
            srvrReady = true;
            cond.notify_one();
        }

        hycast::Socket     sock{srvrSock.accept()};
        hycast::PeerConn   peerConn(sock);
        hycast::ChunkId    id = peerConn.getNotice();
        EXPECT_EQ(chunkId, id);

        peerConn.request(id);

        hycast::WireChunk wireChunk = peerConn.getChunk();
        EXPECT_EQ(chunkId, wireChunk.getId());
        hycast::ChunkSize n = wireChunk.getSize();
        EXPECT_EQ(memChunk.getSize(), n);

        char wireData[n];
        wireChunk.read(wireData);
        EXPECT_EQ(0, ::memcmp(memData, wireData, n));
    }
};

// Tests default construction
TEST_F(PeerConnTest, DefaultConstruction)
{
    hycast::PeerConn peerConn();
}

// Tests a three `Wire` peer connection
TEST_F(PeerConnTest, ThreeWirePeerConn)
{
    hycast::SrvrSock srvrSock(srvrAddr);
    std::thread      srvrThread(&PeerConnTest::runServer, this,
            std::ref(srvrSock));

    //try {
        {
            /*
             * Necessary because `ClntSock` constructor throws if `::connect()`
             * called before `::listen()`
             */
            std::unique_lock<decltype(mutex)> lock{mutex};
            while (!srvrReady)
                cond.wait(lock);
        }

        /*
         * 3 potential port numbers for the client's 2 temporary servers because
         * the initial client connection could use one
         */
        hycast::PortPool portPool(38801, 3);
        hycast::PeerConn peerConn(srvrAddr, portPool);

        EXPECT_EQ(srvrAddr, peerConn.getRmtAddr());

        peerConn.notify(chunkId);

        hycast::ChunkId id = peerConn.getRequest();
        EXPECT_EQ(chunkId, id);

        peerConn.send(memChunk);
    //}
    //catch (const std::exception& ex) {
        //hycast::log_error(ex);
        //abort();
    //}
    //catch (...) {
        //LOG_FATAL("Caught ... exception");
        //abort();
    //}

    srvrThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
