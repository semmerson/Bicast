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
#include "P2pSrvrInfo.h"
#include "RunPar.h"
#include "ThreadException.h"
#include "Tracker.h"

#include <cstring>
#include <gtest/gtest.h>
#include <iostream>

namespace {

using namespace bicast;

bool operator==(const SubP2pMgrPtr lhs, const SubP2pMgr& rhs) {
    return lhs.get() == &rhs;
}
bool operator==(const SubP2pMgr& lhs, const SubP2pMgrPtr rhs) {
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
    SockAddr         subP2pSrvrAddr;
    String           prodName;
    ProdId           prodId;
    char             prodData[PROD_SIZE];
    ProdInfo         prodInfo;
    std::atomic<int> numSubscribers;
    std::atomic<int> numProdId;
    std::atomic<int> numSegId;
    std::atomic<int> numProdReq;
    std::atomic<int> numSegReq;
    std::atomic<int> numProd;
    std::atomic<int> numSeg;
    ThreadEx         threadEx;

    P2pMgrTest()
        : mutex()
        , cond()
        , state(INIT)
        , pubP2pSrvrAddr("127.0.0.1:38800")
        , subP2pSrvrAddr("127.0.0.1:0")
        , prodName("product")
        , prodId(prodName)
        , prodData()
        , prodInfo(prodId, prodName, PROD_SIZE)
        , numSubscribers(0)
        , numProdId(0)
        , numSegId(0)
        , numProdReq(0)
        , numSegReq(0)
        , numProd(0)
        , numSeg(0)
        , threadEx()
    {
        RunPar::maxNumPeers = 8;
        RunPar::maxSegSize = SEG_SIZE;
        RunPar::peerEvalInterval = std::chrono::minutes(1);
        RunPar::p2pSrvrQSize = 5;
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

    void notify(BaseP2pMgrPtr p2pMgr) {
        p2pMgr->notify(prodId);
        for (ProdSize offset = 0; offset < sizeof(prodData); offset += SEG_SIZE) {
            DataSegId dataSegId(prodId, offset);
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

    int maxRecvSeg() {
        return numSubscribers * DataSeg::numSegs(prodInfo.getSize());
    }

public:
    void start() {}
    void stop() {}
    void run() {}
    void halt() {}

    void runP2pMgr(BaseP2pMgrPtr p2pMgr) {
        try {
            p2pMgr->run();
        }
        catch (const std::exception& ex) {
            threadEx.set();
        }
    }

    P2pSrvrInfo getP2pSrvrInfo() const override {
        return P2pSrvrInfo{};
    }

    SockAddr getP2pSrvrAddr() const override {
        return SockAddr{};
    }

    void waitForPeer() {}

    void addProd(
            const String& filePath,
            const String& prodName) const {
    }

    ProdIdSet subtract(ProdIdSet rhs) const override {
        return ProdIdSet{};
    }

    ProdIdSet getProdIds() const override {
        return ProdIdSet{};
    }

    bool shouldRequest(const ProdId index) override {
        EXPECT_TRUE(index == this->prodId);
        if (++numProdId == numSubscribers)
            orState(PROD_NOTICE_RCVD);
        return true;
    }

    bool shouldRequest(const DataSegId segId) override {
        DataSegId expect(prodId, segId.offset);
        EXPECT_EQ(expect, segId);
        if (++numSegId == maxRecvSeg())
            orState(SEG_NOTICE_RCVD);
        return true;
    }

    ProdInfo recvRequest(const ProdId request) override {
        //ASSERT_EQ(prodId, request);     // Doesn't compile
        //ASSERT_TRUE(prodId == request); // Doesn't compile
        EXPECT_TRUE(prodId == request);
        if (++numProdReq == numSubscribers)
            orState(PROD_REQUEST_RCVD);
        return prodInfo;
    }

    DataSeg recvRequest(const DataSegId segId) override {
        EXPECT_EQ(0, segId.offset%SEG_SIZE);
        EXPECT_LE(segId.offset, sizeof(prodData));
        if (++numSegReq == maxRecvSeg())
            orState(SEG_REQUEST_RCVD);
        return DataSeg(segId, sizeof(prodData), prodData+segId.offset);
    }

    void recvP2pData(const ProdInfo prodInfo) {
        EXPECT_EQ(this->prodInfo, prodInfo);
        if (++numProd == numSubscribers)
            //LOG_DEBUG("Setting product information received");
            orState(PROD_INFO_RCVD);
        LOG_DEBUG("numProd=%d, numSubscribers=%d", (int)numProd, (int)numSubscribers);
    }

    void recvP2pData(const DataSeg dataSeg) {
        const auto offset = dataSeg.getId().offset;
        DataSeg expect(dataSeg.getId(), prodInfo.getSize(), prodData+offset);
        EXPECT_EQ(expect, dataSeg);
        if (++numSeg == maxRecvSeg())
            //LOG_DEBUG("Setting data segment received");
            orState(SEG_RCVD);
        LOG_DEBUG("numSeg=%d, numSubscribers=%d", (int)numSeg, (int)numSubscribers);
    }

    void getPduCounts(
            long& numMcastOrig,
            long& numP2pOrig,
            long& numMcastDup,
            long& numP2pDup) const noexcept {
    }

    long getTotalProds() const noexcept {
        return 0;
    }

    long long getTotalBytes() const noexcept {
        return 0;
    }

    double getTotalLatency() const noexcept {
        return 0;
    }
};

#if 0
// Tests construction of publishing P2P manager
TEST_F(P2pMgrTest, PubP2pMgrCtor)
{
    auto pubP2pMgr = PubP2pMgr::create(*static_cast<PubNode*>(this), pubP2pSrvrAddr, 8, 8, 60);
}
#endif

#if 1
// Tests a single subscriber
TEST_F(P2pMgrTest, SingleSubscriber)
{
    numSubscribers = 1;

    Tracker    tracker{};

    LOG_NOTE("Creating publishing P2P manager");
    RunPar::p2pSrvrAddr = pubP2pSrvrAddr;
    auto       pubP2pMgr = PubP2pMgr::create(tracker, *this);
    Thread     pubThread(&P2pMgrTest::runP2pMgr, this, pubP2pMgr);

    try {
        LOG_NOTE("Getting information on publishing P2P-server");
        const auto srvrInfo = pubP2pMgr->getSrvrInfo();
        LOG_NOTE("Adding information on publishing P2P-server to tracker");
        EXPECT_NO_THROW(tracker.insert(srvrInfo));

        LOG_NOTE("Creating subscribing P2P manager");
        RunPar::p2pSrvrAddr = subP2pSrvrAddr;
        auto       subP2pMgr = SubP2pMgr::create(tracker, *this);
        LOG_NOTE("Starting subscribing P2P manager");
        Thread     subThread(&P2pMgrTest::runP2pMgr, this, subP2pMgr);

        try {
            LOG_NOTE("Waiting for a subscribing peer to connect");
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
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
            subP2pMgr->halt();
            subThread.join();
            throw;
        }
    }
    catch (const std::exception& ex) {
        LOG_ERROR(ex);
        pubP2pMgr->halt();
        pubThread.join();
        throw;
    }
}
#endif

#if 1
// Tests two, daisy-chained subscribers
TEST_F(P2pMgrTest, TwoDaisyChained)
{
    numSubscribers = 2;

    Tracker     tracker1{};

    //LOG_DEBUG("Creating publishing P2P manager");
    RunPar::maxNumPeers = 1;
    RunPar::pubSrvrQSize = 2;
    RunPar::p2pSrvrAddr = pubP2pSrvrAddr;
    auto    pubP2pMgr = PubP2pMgr::create(tracker1, *this);
    Thread  pubThread(&P2pMgrTest::runP2pMgr, this, pubP2pMgr);

    tracker1.insert(pubP2pMgr->getSrvrInfo());

    //LOG_DEBUG("Creating first subscribing P2P manager");
    RunPar::p2pSrvrAddr = subP2pSrvrAddr;
    RunPar::maxNumPeers = 2;
    auto    subP2pMgr1 = SubP2pMgr::create(tracker1, *this);
    Thread  subThread1(&P2pMgrTest::runP2pMgr, this, subP2pMgr1);

    /*
     * This test function can freeze if `pubP2pMgr->waitForSrvrPeer()` is used and it returns before
     * `subP2pMgr1` modifies its tier number.
     */
    //LOG_DEBUG("Waiting for first subscriber to connect");
    subP2pMgr1->waitForClntPeer();

    Tracker tracker2{};
    tracker2.insert(subP2pMgr1->getSrvrInfo());

    //LOG_DEBUG("Creating second subscribing P2P manager");
    RunPar::maxNumPeers = 1;
    auto    subP2pMgr2 = SubP2pMgr::create(tracker2, *this);
    Thread  subThread2(&P2pMgrTest::runP2pMgr, this, subP2pMgr2);

    //LOG_DEBUG("Waiting for second subscriber to connect");
    subP2pMgr2->waitForClntPeer();

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
    ::abort();
}

int main(int argc, char **argv) {
  RunPar::init(argc, argv);
  using namespace bicast;

  log_setName(::basename(argv[0]));
  log_setLevel(LogLevel::TRACE);

  std::set_terminate(&myTerminate);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
