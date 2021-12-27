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
#include "P2pMgr.h"

#include <cstring>
#include <gtest/gtest.h>
#include <iostream>

namespace {

using namespace hycast;

bool operator==(const SubP2pMgrPtr lhs, const SubP2pMgr& rhs) {
    return lhs.get() == &rhs;
}
bool operator==(const SubP2pMgr& lhs, const SubP2pMgrPtr rhs) {
    return &lhs == rhs.get();
}

/// The fixture for testing class `P2pMgr`
class P2pMgrTest : public ::testing::Test, public SubNode
{
protected:
    using State = enum {
        INIT              =    0,
        PROD_NOTICE_RCVD  =  0x1,
        SEG_NOTICE_RCVD   =  0x2,
        PROD_REQUEST_RCVD = 0x04,
        SEG_REQUEST_RCVD  = 0x08,
        PROD_INFO_RCVD    = 0x10,
        DATA_RCVD         = 0x20,
        DONE = PROD_NOTICE_RCVD  |
               SEG_NOTICE_RCVD   |
               PROD_REQUEST_RCVD |
               SEG_REQUEST_RCVD  |
               PROD_INFO_RCVD    |
               DATA_RCVD
    };
    Mutex            mutex;
    Cond             cond;
    unsigned         state;
    static constexpr SegSize SEG_SIZE = DataSeg::CANON_DATASEG_SIZE;
    SockAddr         pubPeerSrvrAddr;
    SockAddr         localSrvrAddr;
    ProdIndex        prodIndex;
    char             data[1000];
    ProdInfo         prodInfo;
    SubP2pMgrPtr     testP2pMgrPtr;

    P2pMgrTest()
        : mutex()
        , cond()
        , state(INIT)
        , pubPeerSrvrAddr("127.0.0.1:38800")
        , localSrvrAddr("127.0.0.1:0")
        , prodIndex(1)
        , data()
        , prodInfo(prodIndex, "product", sizeof(data))
        , testP2pMgrPtr()
    {
        ::memset(data, 0xbd, sizeof(data));
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

    void notify(std::shared_ptr<P2pMgr> p2pMgr) {
        p2pMgr->notify(prodIndex);
        for (ProdSize offset = 0; offset < sizeof(data); offset += SEG_SIZE) {
            DataSegId dataSegId(prodIndex, offset);
            p2pMgr->notify(dataSegId);
        }
    }

    void orState(const State state)
    {
        this->state |= state;
        cond.notify_all();
    }

    void waitForState(const unsigned nextState)
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (state != nextState)
            cond.wait(lock);
    }

public:
    bool recvNotice(
            const ProdIndex index,
            SubP2pMgr&      p2pMgr) override {
        EXPECT_TRUE(index == this->prodIndex);
        if (p2pMgr.getPeerSrvrAddr() == testP2pMgrPtr->getPeerSrvrAddr())
            orState(PROD_NOTICE_RCVD);
        return true;
    }

    bool recvNotice(
            const DataSegId segId,
            SubP2pMgr&      p2pMgr) override {
        static    ProdSize offset = 0;
        DataSegId expect(prodIndex, offset);
        EXPECT_EQ(expect, segId);
        if (p2pMgr.getPeerSrvrAddr() == testP2pMgrPtr->getPeerSrvrAddr()) {
            offset += SEG_SIZE;
            if (offset >= sizeof(data))
                orState(SEG_NOTICE_RCVD);
        }
        return true;
    }

    ProdInfo recvRequest(
            const ProdIndex request,
            P2pMgr&         p2pMgr) override {
        //ASSERT_EQ(prodIndex, request);     // Doesn't compile
        //ASSERT_TRUE(prodIndex == request); // Doesn't compile
        EXPECT_TRUE(prodIndex == request);
        if (p2pMgr.getPeerSrvrAddr() == testP2pMgrPtr->getPeerSrvrAddr())
            orState(PROD_REQUEST_RCVD);
        return prodInfo;
    }

    DataSeg recvRequest(
            const DataSegId segId,
            P2pMgr&         p2pMgr) override {
        EXPECT_EQ(0, segId.offset%SEG_SIZE);
        EXPECT_LE(segId.offset, sizeof(data));
        if (p2pMgr.getPeerSrvrAddr() == testP2pMgrPtr->getPeerSrvrAddr())
            orState(SEG_REQUEST_RCVD);
        return DataSeg(segId, sizeof(data), data+segId.offset);
    }

    void recvData(
            const ProdInfo prodInfo,
            SubP2pMgr&     p2pMgr) {
        EXPECT_EQ(this->prodInfo, prodInfo);
        if (p2pMgr.getPeerSrvrAddr() == testP2pMgrPtr->getPeerSrvrAddr())
            orState(PROD_INFO_RCVD);
    }

    void recvData(
            const DataSeg dataSeg,
            SubP2pMgr&    p2pMgr) {
        static    ProdSize offset = 0;
        DataSegId segId(prodIndex, offset);
        DataSeg   expect(segId, sizeof(data), data+segId.offset);
        EXPECT_EQ(expect, dataSeg);
        if (p2pMgr.getPeerSrvrAddr() == testP2pMgrPtr->getPeerSrvrAddr()) {
            offset += SEG_SIZE;
            if (offset >= sizeof(data))
                orState(DATA_RCVD);
        }
    }
};

#if 0
// Tests construction of publisher's P2P manager
TEST_F(P2pMgrTest, PubP2pMgrCtor)
{
    auto pubP2pMgr = hycast::P2pMgr::create(*this, pubPeerSrvrAddr, 8,
            SEG_SIZE);
}
#endif

#if 0
// Tests a single subscriber
TEST_F(P2pMgrTest, SingleSubscriber)
{
    auto       pubP2pMgrPtr = P2pMgr::create(*this, pubPeerSrvrAddr, 8, SEG_SIZE);
    Tracker    tracker{};
    tracker.insert(pubPeerSrvrAddr);
    testP2pMgrPtr = SubP2pMgr::create(*this, tracker, localSrvrAddr, 8,
            SEG_SIZE);

    pubP2pMgrPtr->waitForSrvrPeer();
    notify(pubP2pMgrPtr);
    waitForState(PROD_NOTICE_RCVD  |
               SEG_NOTICE_RCVD   |
               PROD_INFO_RCVD    |
               DATA_RCVD);
}
#endif

#if 1
// Tests two, daisy-chained subscribers
TEST_F(P2pMgrTest, TwoDaisyChained)
{
    auto       pubP2pMgrPtr = P2pMgr::create(*this, pubPeerSrvrAddr, 1,
            SEG_SIZE);

    Tracker    tracker1{};
    tracker1.insert(pubPeerSrvrAddr);
    auto       subP2pMgrPtr1 = SubP2pMgr::create(*this, tracker1, localSrvrAddr,
            2, SEG_SIZE);

    pubP2pMgrPtr->waitForSrvrPeer();

    Tracker    tracker2{};
    tracker2.insert(subP2pMgrPtr1->getPeerSrvrAddr());
    testP2pMgrPtr = SubP2pMgr::create(*this, tracker2, localSrvrAddr,
            1, SEG_SIZE);

    subP2pMgrPtr1->waitForSrvrPeer();

    notify(pubP2pMgrPtr);
    waitForState(PROD_NOTICE_RCVD  |
               SEG_NOTICE_RCVD   |
               PROD_INFO_RCVD    |
               DATA_RCVD);
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
