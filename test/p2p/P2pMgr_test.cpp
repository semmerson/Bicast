/**
 * This file tests class `P2pMgr`.
 *
 * Copyright 2021 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: P2pMgr_test.cpp
 * Created On: Oct 3, 2021
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "logging.h"
#include "Node.h"
#include "P2pMgr.h"
#include "ThreadException.h"

#include <cstring>
#include <gtest/gtest.h>
#include <iostream>

namespace {

using namespace hycast;

bool operator==(const SubP2pMgr::Pimpl lhs, const SubP2pMgr& rhs) {
    return lhs.get() == &rhs;
}
bool operator==(const SubP2pMgr& lhs, const SubP2pMgr::Pimpl rhs) {
    return &lhs == rhs.get();
}

/// The fixture for testing class `P2pMgr`
class P2pMgrTest : public ::testing::Test, public PubNode, public SubNode
{
protected:
    using State = enum {
        INIT              =    0,
        PROD_NOTICE_RCVD  =  0x1,
        SEG_NOTICE_RCVD   =  0x2,
        PROD_REQUEST_RCVD = 0x04,
        SEG_REQUEST_RCVD  = 0x08,
        PROD_INFO_RCVD    = 0x10,
        SEG_RCVD          = 0x20,
        DONE = PROD_NOTICE_RCVD  |
               SEG_NOTICE_RCVD   |
               PROD_REQUEST_RCVD |
               SEG_REQUEST_RCVD  |
               PROD_INFO_RCVD    |
               SEG_RCVD
    };
    Mutex            mutex;
    Cond             cond;
    unsigned         state;
    static constexpr SegSize SEG_SIZE = 1200;
    static constexpr ProdSize PROD_SIZE = 1000;
    SockAddr         pubP2pSrvrAddr;
    SockAddr         localSrvrAddr;
    ProdId        prodIndex;
    char             prodData[PROD_SIZE];
    ProdInfo         prodInfo;
    std::atomic<int> subscriberCount;
    std::atomic<int> numProdInfos;
    std::atomic<int> numDataSegs;
    ThreadEx         threadEx;

    P2pMgrTest()
        : mutex()
        , cond()
        , state(INIT)
        , pubP2pSrvrAddr("127.0.0.1:38800")
        , localSrvrAddr("127.0.0.1:0")
        , prodIndex(1)
        , prodData()
        , prodInfo(prodIndex, "product", PROD_SIZE)
        , subscriberCount(0)
        , numProdInfos(0)
        , numDataSegs(0)
        , threadEx()
    {
        DataSeg::setMaxSegSize(SEG_SIZE);
        int i = 0;
        for (ProdSize offset = 0; offset < PROD_SIZE; offset += SEG_SIZE) {
            auto str = std::to_string(i++);
            int c = str[str.length()-1];
            ::memset(prodData+offset, c, DataSeg::size(PROD_SIZE, offset));
        }
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

    void notify(P2pMgr::Pimpl p2pMgr) {
        p2pMgr->notify(prodIndex);
        for (ProdSize offset = 0; offset < sizeof(prodData); offset += SEG_SIZE) {
            DataSegId dataSegId(prodIndex, offset);
            p2pMgr->notify(dataSegId);
        }
    }

    void orState(const State state)
    {
        Guard guard{mutex};
        this->state |= state;
        cond.notify_all();
    }

    void waitForState(const unsigned nextState)
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        cond.wait(lock, [&]{return state == nextState || threadEx;});
    }

public:
    void start() {}
    void stop() {}
    void run() {}
    void halt() {}

    void runP2pMgr(P2pMgr::Pimpl p2pMgr) {
        try {
            p2pMgr->run();
        }
        catch (const std::exception& ex) {
            threadEx.set(ex);
        }
    }

    SockAddr getP2pSrvrAddr() const override {
        return SockAddr{};
    }

    void waitForPeer() {}

    bool shouldRequest(const ProdId index) override {
        EXPECT_TRUE(index == this->prodIndex);
        orState(PROD_NOTICE_RCVD);
        return true;
    }

    bool shouldRequest(const DataSegId segId) override {
        DataSegId expect(prodIndex, segId.offset);
        EXPECT_EQ(expect, segId);
        if (segId.offset+SEG_SIZE >= prodInfo.getSize())
            orState(SEG_NOTICE_RCVD);
        return true;
    }

    ProdInfo recvRequest(const ProdId request) override {
        //ASSERT_EQ(prodIndex, request);     // Doesn't compile
        //ASSERT_TRUE(prodIndex == request); // Doesn't compile
        EXPECT_TRUE(prodIndex == request);
        orState(PROD_REQUEST_RCVD);
        return prodInfo;
    }

    DataSeg recvRequest(const DataSegId segId) override {
        EXPECT_EQ(0, segId.offset%SEG_SIZE);
        EXPECT_LE(segId.offset, sizeof(prodData));
        orState(SEG_REQUEST_RCVD);
        return DataSeg(segId, sizeof(prodData), prodData+segId.offset);
    }

    void recvP2pData(const ProdInfo prodInfo) {
        EXPECT_EQ(this->prodInfo, prodInfo);
        //LOG_DEBUG("numProdInfos=%d, subscriberCount=%d", (int)numProdInfos, (int)subscriberCount);
        if (++numProdInfos == subscriberCount) {
            //LOG_DEBUG("Setting product information received");
            orState(PROD_INFO_RCVD);
        }
    }

    void recvP2pData(const DataSeg dataSeg) {
        const auto offset = dataSeg.getId().offset;
        DataSeg expect(dataSeg.getId(), prodInfo.getSize(), prodData+offset);
        EXPECT_EQ(expect, dataSeg);
        //LOG_DEBUG("numDataSegs=%d, subscriberCount=%d", (int)numDataSegs, (int)subscriberCount);
        if (offset+SEG_SIZE >= prodInfo.getSize() && ++numDataSegs == subscriberCount) {
            //LOG_DEBUG("Setting data segment received");
            orState(SEG_RCVD);
        }
    }
};

#if 1
// Tests construction of publisher's P2P manager
TEST_F(P2pMgrTest, PubP2pMgrCtor)
{
    auto pubP2pMgr = hycast::P2pMgr::create(*this, pubP2pSrvrAddr, 8, SEG_SIZE);
}
#endif

#if 1
// Tests a single subscriber
TEST_F(P2pMgrTest, SingleSubscriber)
{
    subscriberCount = 1;

    LOG_NOTE("Creating publishing P2P manager");
    auto       pubP2pMgr = P2pMgr::create(*this, pubP2pSrvrAddr, 8, SEG_SIZE);
    Thread     pubThread(&P2pMgrTest::runP2pMgr, this, pubP2pMgr);

    Tracker    tracker{};
    LOG_NOTE("Getting socket address of publishing P2P manager");
    const auto addr = pubP2pMgr->getSrvrAddr();
    LOG_NOTE("Adding socket address of publishing P2P manager to tracker");
    tracker.insert(addr);

    LOG_NOTE("Creating subscribing P2P manager");
    auto       subP2pMgr = SubP2pMgr::create(*this, tracker, localSrvrAddr, 8, SEG_SIZE);
    LOG_NOTE("Starting subscribing P2P manager");
    Thread     subThread(&P2pMgrTest::runP2pMgr, this, subP2pMgr);

    LOG_NOTE("Waiting for subscriber to connect");
    pubP2pMgr->waitForSrvrPeer();

    LOG_NOTE("Notifying subscriber");
    notify(pubP2pMgr);

    LOG_NOTE("Waiting for termination");
    waitForState(PROD_NOTICE_RCVD  |
               SEG_NOTICE_RCVD   |
               PROD_REQUEST_RCVD |
               SEG_REQUEST_RCVD  |
               PROD_INFO_RCVD    |
               SEG_RCVD);

    EXPECT_FALSE(threadEx);

    subP2pMgr->halt();
    pubP2pMgr->halt();

    subThread.join();
    pubThread.join();
}
#endif

#if 1
// Tests two, daisy-chained subscribers
TEST_F(P2pMgrTest, TwoDaisyChained)
{
    subscriberCount = 2;

    //LOG_DEBUG("Creating publishing P2P manager");
    auto       pubP2pMgr = P2pMgr::create(*this, pubP2pSrvrAddr, 1, SEG_SIZE);
    Thread     pubThread(&P2pMgrTest::runP2pMgr, this, pubP2pMgr);

    Tracker    tracker1{};
    //LOG_DEBUG("Inserting address of publishing P2P server");
    tracker1.insert(pubP2pSrvrAddr);
    //LOG_DEBUG("Creating first subscribing P2P manager");
    auto       subP2pMgr1 = SubP2pMgr::create(*this, tracker1, localSrvrAddr, 2, SEG_SIZE);
    Thread     subThread1(&P2pMgrTest::runP2pMgr, this, subP2pMgr1);

    //LOG_DEBUG("Waiting for first subscriber to connect");
    pubP2pMgr->waitForSrvrPeer();

    Tracker tracker2{};
    //LOG_DEBUG("Inserting address of subscribing P2P server");
    tracker2.insert(subP2pMgr1->getSrvrAddr());
    //LOG_DEBUG("Creating second subscribing P2P manager");
    auto    subP2pMgr2 = SubP2pMgr::create(*this, tracker2, localSrvrAddr, 1, SEG_SIZE);
    Thread     subThread2(&P2pMgrTest::runP2pMgr, this, subP2pMgr2);

    //LOG_DEBUG("Waiting for second subscriber to connect");
    subP2pMgr1->waitForSrvrPeer();

    //LOG_DEBUG("Notifying publishing P2P manager");
    notify(pubP2pMgr);
    //LOG_DEBUG("Waiting for termination");
    waitForState(PROD_NOTICE_RCVD  |
               SEG_NOTICE_RCVD   |
               PROD_REQUEST_RCVD |
               SEG_REQUEST_RCVD  |
               PROD_INFO_RCVD    |
               SEG_RCVD);

    EXPECT_FALSE(threadEx);

    subP2pMgr2->halt();
    subP2pMgr1->halt();
    pubP2pMgr->halt();

    subThread2.join();
    subThread1.join();
    pubThread.join();
}
#endif

}  // namespace

static void myTerminate()
{
    if (!std::current_exception()) {
        LOG_FATAL("terminate() called without an active exception");
    }
    else {
        LOG_FATAL("terminate() called with an active exception");
        try {
            std::rethrow_exception(std::current_exception());
        }
        catch (const std::exception& ex) {
            LOG_FATAL(ex);
        }
        catch (...) {
            LOG_FATAL("Exception is unknown");
        }
    }
    abort();
}

int main(int argc, char **argv) {
  hycast::log_setName(::basename(argv[0]));
  hycast::log_setLevel(hycast::LogLevel::TRACE);

  std::set_terminate(&myTerminate);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
