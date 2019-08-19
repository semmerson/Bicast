/**
 * This file tests class `PeerSetMgr`.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: PeerSetMgr_test.cpp
 * Created On: Jul 1, 2019
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "P2pMgr.h"
#include "PeerFactory.h"
#include "SockAddr.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>
#include <unistd.h>

namespace {

/// The fixture for testing class `PeerSetMgr`
class P2pMgrTest : public ::testing::Test, public hycast::PeerMsgRcvr
{
protected:
    hycast::SockAddr        srcAddr;
    hycast::SockAddr        snkAddr;
    std::mutex              mutex;
    std::condition_variable cond;
    enum {
        INIT,
        DONE
    }                       state;
    hycast::ChunkId         chunkId;
    char                    memData[1000];
    hycast::MemChunk        memChunk;
    hycast::PortPool        portPool;
    hycast::ServerPool      srcSrvrPool;
    hycast::ServerPool      snkSrvrPool;

    // You can remove any or all of the following functions if its body
    // is empty.

    P2pMgrTest()
        : srcAddr{"localhost:38800"}
        , snkAddr{"localhost:38801"}
        , mutex{}
        , cond{}
        , state{INIT}
        , chunkId{1}
        , memData{0}
        , memChunk(chunkId, sizeof(memData), memData)
        , portPool(38802, 38803)
        , srcSrvrPool(std::set<hycast::SockAddr>{})
        , snkSrvrPool(std::set<hycast::SockAddr>{srcAddr})
    {
        // You can do set-up work for each test here.
    }

    virtual ~P2pMgrTest()
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
        char                    wireData[n];

        wireChunk.read(wireData);
        for (int i = 0; i < n; ++i)
            EXPECT_EQ(memData[i], wireData[i]);

        {
            std::lock_guard<decltype(mutex)> lock{mutex};
            state = DONE;
            cond.notify_one();
        }
    }
};

// Tests simple construction
TEST_F(P2pMgrTest, SimpleConstruction)
{
    std::set<hycast::SockAddr> servers{}; // Empty => no `factory.connect()`
    hycast::ServerPool         serverPool{servers};
    hycast::P2pMgr             p2pMgr{srcAddr, 0, portPool, 0, serverPool,
        *this};
}

// Tests exchanging data
TEST_F(P2pMgrTest, DataExchange)
{
    // Start source
    hycast::P2pMgr srcP2pMgr =
            hycast::P2pMgr(srcAddr, 0, portPool, 1, srcSrvrPool, *this);
    std::thread srcThread{srcP2pMgr};

    try {
        // Start sink
        hycast::P2pMgr snkP2pMgr =
                hycast::P2pMgr(snkAddr, 0, portPool, 1, snkSrvrPool, *this);
        std::thread snkThread{snkP2pMgr};

        try {
            // Wait for the sink to connect to the source
            while (srcP2pMgr.size() == 0)
                ::usleep(200000);

            // Start an exchange
            srcP2pMgr.notify(chunkId);

            // Wait for the exchange to complete
            {
                std::unique_lock<decltype(mutex)> lock{mutex};
                while (state != DONE)
                    cond.wait(lock);
            }

            snkP2pMgr.halt();
            snkThread.join();

            srcP2pMgr.halt();
            srcThread.join();
        } // Sink thread created
        catch (const std::exception& ex) {
            hycast::log_error(ex);
            if (snkThread.joinable()) {
                snkP2pMgr.halt();
                snkThread.join();
            }
            throw;
        }
    } // Source thread created
    catch (const std::exception& ex) {
        hycast::log_error(ex);
        if (srcThread.joinable()) {
            srcP2pMgr.halt();
            srcThread.join();
        }
        throw;
    }
}

} // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
