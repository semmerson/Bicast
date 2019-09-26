/**
 * This file tests class `PeerThreadPool`.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: PeerThreadPool_test.cpp
 * Created On: Aug 8, 2019
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "PeerFactory.h"
#include "PeerThreadPool.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `PeerThreadPool`
class PeerThreadPoolTest : public ::testing::Test, public hycast::PeerMsgRcvr
{
protected:
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;
    typedef std::condition_variable Cond;

    hycast::SockAddr        srvrAddr;
    Mutex                   mutex;
    Cond                    cond;
    typedef enum {
        INIT,
        SERVER_PEER_READY,
        DONE
    }                       State;
    State                   state;
    hycast::ChunkId         chunkId;
    char                    memData[1000];
    hycast::MemChunk        memChunk;

    // You can remove any or all of the following functions if its body
    // is empty.

    PeerThreadPoolTest()
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

    virtual ~PeerThreadPoolTest()
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
    void setState(const State newState)
    {
        Guard guard{mutex};

        state = newState;
        cond.notify_one();
    }

    void waitForState(const State targetState)
    {
        Lock lock{mutex};

        while (state != targetState)
            cond.wait(lock);
    }

public:
    bool shouldRequest(
            const hycast::ChunkId& notice,
            hycast::Peer&          peer)
    {
        EXPECT_EQ(chunkId, notice);
        return true;
    }

    hycast::MemChunk get(
            const hycast::ChunkId& request,
            hycast::Peer&          peer)
    {
        EXPECT_EQ(chunkId, request);
        return memChunk;
    }

    void hereIs(
            hycast::WireChunk& wireChunk,
            hycast::Peer&      peer)
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

    void runServer(
            hycast::PeerFactory    factory,
            hycast::PeerThreadPool pool)
    {
        try {
            hycast::Peer srvrPeer = factory.accept();
            EXPECT_TRUE(pool.execute(srvrPeer));

            setState(SERVER_PEER_READY);
        }
        catch (const std::exception& ex) {
            hycast::log_error(ex);
        }
    }
};

// Tests valid construction
TEST_F(PeerThreadPoolTest, ValidConstruction)
{
    hycast::PeerThreadPool pool(0);
}

// Tests only server peer
TEST_F(PeerThreadPoolTest, OnlyServerPeer)
{
    hycast::PeerThreadPool pool(1);
    hycast::PortPool       portPool{38801, 2};
    hycast::PeerFactory    factory(srvrAddr, 1, portPool, *this);
    std::thread            srvrThread(&PeerThreadPoolTest::runServer, this,
            factory, pool);

    // Start a client peer
    hycast::Peer clntPeer = factory.connect(srvrAddr);
    waitForState(SERVER_PEER_READY);
    EXPECT_FALSE(pool.execute(clntPeer));

    srvrThread.join();
}

// Tests data exchange
TEST_F(PeerThreadPoolTest, DataExchange)
{
    hycast::PeerThreadPool pool(2);
    hycast::PortPool       portPool{38801, 2};
    hycast::PeerFactory    factory(srvrAddr, 1, portPool, *this);
    std::thread            srvrThread(&PeerThreadPoolTest::runServer, this,
            factory, pool);

    // Start a client peer
    hycast::Peer clntPeer = factory.connect(srvrAddr);
    waitForState(SERVER_PEER_READY);
    EXPECT_TRUE(pool.execute(clntPeer));

    // Start an exchange
    clntPeer.notify(chunkId);

    // Wait for the exchange to complete
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (state != DONE)
            cond.wait(lock);
    }

    srvrThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
