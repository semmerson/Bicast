/**
 * This file tests class `P2pMgr`.
 *
 *       File: P2pMgr_test.cpp
 * Created On: Jul 1, 2019
 *     Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"

#include "error.h"
#include "SockAddr.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <main/p2p-old/P2pMgr.h>
#include <main/p2p-old/PeerFactory.h>
#include <mutex>
#include <thread>
#include <unistd.h>

namespace {

/// The fixture for testing class `P2pMgr`
class P2pMgrTest : public ::testing::Test, public hycast::P2pSub
{
protected:
    hycast::SockAddr        pubAddr;
    hycast::P2pInfo         pubP2pInfo;
    hycast::SockAddr        subAddr;
    hycast::P2pInfo         subP2pInfo;
    std::mutex              mutex;
    std::condition_variable cond;
    typedef enum {
        INIT = 0,
        PROD_NOTICE_RCVD  =   0x4,
        SEG_NOTICE_RCVD   =   0x8,
        PROD_REQUEST_RCVD =  0x10,
        SEG_REQUEST_RCVD  =  0x20,
        PROD_INFO_RCVD    =  0x40,
        SEG_RCVD          =  0x80,
        CLNT_PEER_STOPPED = 0x100,
        SRVR_PEER_STOPPED = 0x200,
        EXCHANGE_COMPLETE =
               PROD_NOTICE_RCVD  |
               SEG_NOTICE_RCVD   |
               PROD_REQUEST_RCVD |
               SEG_REQUEST_RCVD  |
               PROD_INFO_RCVD    |
               SEG_RCVD,
        DONE = EXCHANGE_COMPLETE |
               CLNT_PEER_STOPPED |
               SRVR_PEER_STOPPED
    } State;
    State                   state;
    hycast::ProdIndex       prodIndex;
    hycast::ProdSize        prodSize;
    hycast::SegSize         segSize;
    hycast::ProdInfo        prodInfo;
    hycast::SegId           segId;
    hycast::SegInfo         segInfo;
    char                    memData[1000];
    hycast::MemSeg          memSeg;
    std::atomic<unsigned>   numAdded;

    P2pMgrTest()
        : pubAddr{"localhost:3880"}
        , pubP2pInfo()
        , subAddr{"localhost:3883"} // NB: Not a Linux dynamic port number
        , mutex{}
        , cond{}
        , prodIndex{1}
        , state{INIT}
        , prodSize{1000000}
        , segSize{sizeof(memData)}
        , prodInfo{prodIndex, prodSize, "product"}
        , segId(prodIndex, segSize)
        , segInfo(segId, prodSize, segSize)
        , memData{}
        , memSeg{segInfo, memData}
        , numAdded{0}
    {
        pubP2pInfo.sockAddr = pubAddr;
        pubP2pInfo.listenSize = 1;
        pubP2pInfo.maxPeers = 1;

        subP2pInfo = pubP2pInfo;
        subP2pInfo.sockAddr = subAddr;

        ::memset(memData, 0xbd, segSize);
    }

    void setState(const State state) {
        std::lock_guard<decltype(mutex)> lock{mutex};

        this->state = state;
        cond.notify_one();
    }

    void orState(const State state)
    {
        std::lock_guard<decltype(mutex)> guard{mutex};
        this->state = static_cast<State>(this->state | state);
        cond.notify_all();
    }

    void waitForState(const State state) {
        std::unique_lock<decltype(mutex)> lock{mutex};

        while (this->state != state)
            cond.wait(lock);
    }

public:
    // Receiver-side
    bool shouldRequest(hycast::ProdIndex actual)
    {
        EXPECT_TRUE(prodIndex == actual);
        orState(PROD_NOTICE_RCVD);

        return true;
    }

    // Receiver-side
    bool shouldRequest(const hycast::SegId& actual)
    {
        EXPECT_EQ(segId, actual);
        orState(SEG_NOTICE_RCVD);

        return true;
    }

    // Sender-side
    hycast::ProdInfo getProdInfo(hycast::ProdIndex actual)
    {
        EXPECT_TRUE(prodIndex == actual);
        orState(PROD_REQUEST_RCVD);
        return prodInfo;
    }

    // Sender-side
    hycast::MemSeg getMemSeg(const hycast::SegId& actual)
    {
        EXPECT_EQ(segId, actual);
        orState(SEG_REQUEST_RCVD);
        return memSeg;
    }

    // Receiver-side
    bool hereIsP2p(const hycast::ProdInfo actual)
    {
        EXPECT_EQ(prodInfo, actual);
        orState(PROD_INFO_RCVD);

        return true;
    }

    // Receiver-side
    bool hereIsP2p(hycast::TcpSeg& actual)
    {
        const hycast::SegSize size = actual.getSegInfo().getSegSize();
        EXPECT_EQ(segSize, size);

        char buf[size];
        actual.getData(buf);

        EXPECT_EQ(0, ::memcmp(memSeg.data(), buf, segSize));

        orState(SEG_RCVD);

        return true;
    }

    void runP2pMgr(hycast::P2pMgr& p2pMgr) {
        try {
            LOG_DEBUG("Executing p2pMgr");
            p2pMgr();
        }
        catch (const std::exception& ex) {
            LOG_DEBUG(ex, "Caught std::exception");
        }
        catch (...) {
            LOG_DEBUG("Caught ... exception");
            throw;
        }
    }
};

// Tests simple construction
TEST_F(P2pMgrTest, SimpleConstruction)
{
    hycast::P2pMgr pubP2pMgr(pubP2pInfo, *this);
}

// Tests exchanging data between two nodes
TEST_F(P2pMgrTest, DataExchange)
{
    // Start publisher
    hycast::P2pMgr pubP2pMgr(pubP2pInfo, *this);
    std::thread    pubThread(&P2pMgrTest::runP2pMgr, this, std::ref(pubP2pMgr));

    // Start subscriber
    hycast::ServerPool subSrvrPool(std::set<hycast::SockAddr>{pubAddr});
    hycast::P2pMgr     subP2pMgr(subP2pInfo, subSrvrPool, *this);
    std::thread        subThread(&P2pMgrTest::runP2pMgr, this,
            std::ref(subP2pMgr));

    sleep(1);

    // Start an exchange
    pubP2pMgr.notify(prodIndex);
    pubP2pMgr.notify(segId);

    // Wait for the exchange to complete
    waitForState(EXCHANGE_COMPLETE);

    subP2pMgr.halt();
    subThread.join();

    pubP2pMgr.halt();
    pubThread.join();
}

#if 0
// Tests multiple peers
TEST_F(P2pMgrTest, MultiplePeers)
{
    try {
        // Start servers
        hycast::P2pMgr srvrP2pMgrs[3] = {};
        std::thread    srvrThreads[3] = {};

        try {
            for (int i = 0; i < 3; ++i) {
                LOG_NOTE("Creating server %d", i);
                srvrP2pMgrs[i] = hycast::P2pMgr(srvrAddrs[i], 0, 1,
                        srvrSrvrPool, *this);
                LOG_NOTE("Executing server %d", i);
                srvrThreads[i] = std::thread(&P2pMgrTest::runP2pMgr,
                        std::ref(srvrP2pMgrs[i]));
            }
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex, "Server failure");
            throw;
        }

        // Start client
        LOG_NOTE("Starting client");
        hycast::ServerPool clntSrvrPool(std::set<hycast::SockAddr>{
            srvrAddrs[0], srvrAddrs[1], srvrAddrs[2]});
        hycast::P2pMgr     clntP2pMgr(subAddr, 0, 2, clntSrvrPool,
                *this);
        std::thread        clntThread(&P2pMgrTest::runP2pMgr,
                std::ref(clntP2pMgr));

        // Wait for the client to connect to the servers
        LOG_NOTE("Waiting for client to connect to servers");
        while (clntP2pMgr.size() < 2)
            ::usleep(100000);

        // Start an exchange
        LOG_NOTE("Exchanging data");
        for (int i = 0; i < 3; ++i)
            srvrP2pMgrs[i].notify(chunkId);

        // Wait for the exchange to complete
        LOG_NOTE("Waiting for data-exchange to complete");
        waitForState(DONE);

        LOG_NOTE("Stopping client");
        clntP2pMgr.halt();
        clntThread.join();

        for (int i = 0; i < 3; ++i) {
            LOG_NOTE("Stopping server %d", i);
            srvrP2pMgrs[i].halt();
            srvrThreads[i].join();
        }
    }
    catch (const std::exception& ex) {
        LOG_ERROR(ex, "Couldn't test multiple peers");
    }
    catch (...) {
        LOG_ERROR("Caught ... exception");
        throw;
    }
}
#endif

} // namespace

static void myTerminate()
{
    auto exPtr = std::current_exception();

    if (!exPtr) {
        LOG_FATAL("terminate() called without an active exception");
    }
    else {
        try {
            std::rethrow_exception(exPtr);
        }
        catch (const std::exception& ex) {
            LOG_FATAL(ex);
        }
        catch (...) {
            LOG_FATAL("terminate() called with a non-standard exception");
        }
    }

    abort();
}

int main(int argc, char **argv)
{
  hycast::log_setName(::basename(argv[0]));
  hycast::log_setLevel("debug");

  std::set_terminate(&myTerminate);
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
