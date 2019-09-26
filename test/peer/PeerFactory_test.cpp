/**
 * This file tests class `PeerFactory`.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: PeetFactory_test.cpp
 * Created On: June 6, 2019
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "PeerFactory.h"
#include "SockAddr.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `Peer`
class PeerTest : public ::testing::Test, public hycast::PeerMsgRcvr
{
protected:
    friend class Receiver;

    hycast::SockAddr        srvrAddr;
    std::mutex              mutex;
    std::condition_variable cond;
    enum {
        INIT,
        DONE
    }                       state;
    hycast::ChunkId         chunkId;
    char                    memData[1000];
    hycast::MemChunk        memChunk;

    // You can remove any or all of the following functions if its body
    // is empty.

    PeerTest()
        : srvrAddr{"localhost:38800"}
        , mutex{}
        , cond{}
        , state{INIT}
        , chunkId{1}
        , memData{0}
        , memChunk(chunkId, sizeof(memData), memData)
    {
        // You can do set-up work for each test here.
    }

    virtual ~PeerTest()
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
    bool shouldRequest(
            const hycast::ChunkId&  notice,
            const hycast::SockAddr& rmtSockAddr)
    {
        EXPECT_EQ(chunkId, notice);
        return true;
    }

    hycast::MemChunk get(
            const hycast::ChunkId&  request,
            const hycast::SockAddr& rmtSockAddr)
    {
        EXPECT_EQ(chunkId, request);
        return memChunk;
    }

    void hereIs(
            hycast::WireChunk       wireChunk,
            const hycast::SockAddr& rmtSockAddr)
    {
        const hycast::ChunkSize n = wireChunk.getSize();
        EXPECT_EQ(memChunk.getSize(), n);

        char wireData[n];
        wireChunk.read(wireData);
        EXPECT_EQ(0, ::memcmp(memData, wireData, n));

        {
            std::lock_guard<decltype(mutex)> lock(mutex);
            state = DONE;
            cond.notify_one();
        }
    }

    bool shouldRequest(
            const hycast::ChunkId& notice,
            hycast::Peer           peer)
    {
        EXPECT_EQ(chunkId, notice);
        return true;
    }

    hycast::MemChunk get(
            const hycast::ChunkId& request,
            hycast::Peer           peer)
    {
        EXPECT_EQ(chunkId, request);
        return memChunk;
    }

    void hereIs(
            hycast::WireChunk wireChunk,
            hycast::Peer      peer)
    {
        const hycast::ChunkSize n = wireChunk.getSize();
        EXPECT_EQ(memChunk.getSize(), n);

        char wireData[n];
        wireChunk.read(wireData);
        EXPECT_EQ(0, ::memcmp(memData, wireData, n));

        {
            std::lock_guard<decltype(mutex)> lock(mutex);
            state = DONE;
            cond.notify_one();
        }
    }

    void runServer(hycast::PeerFactory& factory)
    {
        try {
            hycast::Peer srvrPeer = factory.accept();
            if (srvrPeer)
                srvrPeer();
        }
        catch (const std::exception& ex) {
            hycast::log_error(ex);
        }
    }
};

// Tests closing the factory
TEST_F(PeerTest, FactoryClosure)
{
    // Start a peer-server. Calls `::listen()`.
    hycast::PortPool portPool{38801, 2};
    hycast::PeerFactory factory(srvrAddr, 1, portPool, *this);
    std::thread srvrThread(&PeerTest::runServer, this, std::ref(factory));

    // Ensure `factory.accept()` called to test shutdown() effect on accept()
    ::sleep(1);

    // Close the factory
    factory.close();

    srvrThread.join();
}

// Tests complete exchange (notice, request, delivery)
TEST_F(PeerTest, Exchange)
{
    hycast::PortPool portPool{38801, 2};

    // Start a peer-server. Calls `::listen()`.
    hycast::PeerFactory factory(srvrAddr, 1, portPool, *this);
    std::thread srvrThread(&PeerTest::runServer, this, std::ref(factory));

    // Start a client peer
    hycast::Peer clntPeer = factory.connect(srvrAddr);
    std::thread  clntThread(clntPeer); // `clntPeer` is connected

    // Start an exchange
    clntPeer.notify(chunkId);

    // Wait for the exchange to complete
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (state != DONE)
            cond.wait(lock);
    }

    // Causes `clntPeer()` to return and `srvrThread` to terminate
    clntPeer.halt();
    clntThread.join();
    srvrThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
