#include "config.h"

#include "BicastProto.h"
#include "logging.h"
#include "P2pSrvrInfo.h"
#include "Peer.h"
#include "Tracker.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <signal.h>
#include <thread>

namespace {

using namespace bicast;

/// The fixture for testing class `Peer`
class PeerTest : public ::testing::Test, public Peer::SubMgr // Peer::SubMgr has all functions
{
protected:
    typedef enum {
        INIT = 0,
        LISTENING             =    0x1,
        SRVR_INFO_RCVD        =    0x2,
        SRVR_INFO_NOTICE_RCVD =    0x8,
        PROD_NOTICE_RCVD      =   0x10,
        SEG_NOTICE_RCVD       =   0x20,
        PROD_REQUEST_RCVD     =   0x40,
        SEG_REQUEST_RCVD      =   0x80,
        PROD_INFO_RCVD        =  0x100,
        DATA_SEG_RCVD         =  0x200,
        PROD_INFO_MISSED      =  0x400,
        DATA_SEG_MISSED       =  0x800
    } State;
    State             state;
    SockAddr          pubAddr;
    SockAddr          subAddr;
    Tracker           pubTracker;
    Tracker           subTracker;
    P2pSrvrInfo       pubSrvrInfo;
    P2pSrvrInfo       subSrvrInfo;
    Thread::id        subThreadId; // ID of thread on which subscribing peer is created
    std::mutex        mutex;
    Cond              cond;
    ProdId            prodIds[2];
    ProdId            prodId;
    ProdSize          prodSize;
    SegSize           segSize;
    ProdInfo          prodInfos[2];
    ProdInfo          prodInfo;
    DataSegId         segIds[2];
    DataSegId         segId;
    char              memData[1100];
    DataSeg           dataSegs[2];
    DataSeg           dataSeg;
    std::atomic<bool> skipping;
    int               prodNoticeCount;
    int               segNoticeCount;
    int               prodRequestCount;
    int               segRequestCount;
    int               prodDataCount;
    int               segDataCount;
    Thread            pubPeerThread;
    int               numTrackerRcvd;

    PeerTest()
        : state(INIT)
        , pubAddr("localhost:38800")
        , subAddr("localhost:38801")
        , pubTracker()
        , subTracker()
        , pubSrvrInfo(pubAddr, 1, 0)
        , subSrvrInfo(subAddr, 1, 1)
        , mutex()
        , cond()
        , prodIds()
        , prodId()
        , prodSize(1000000)
        , segSize(sizeof(memData))
        , prodInfos()
        , prodInfo()
        , segIds()
        , segId()
        , memData()
        , dataSegs()
        , dataSeg()
        , skipping(false)
        , prodNoticeCount(0)
        , segNoticeCount(0)
        , prodRequestCount(0)
        , segRequestCount(0)
        , prodDataCount(0)
        , segDataCount(0)
        , numTrackerRcvd(0)
    {
        DataSeg::setMaxSegSize(sizeof(memData));
        String prodNames[] = {"product1", "product2"};

        ::memset(memData, 0xbd, segSize);

        for (int i = 0; i < 2; ++i) {
            prodIds[i] = ProdId(prodNames[i]);
            prodInfos[i] = ProdInfo(prodIds[i], prodNames[i], prodSize);
            segIds[i] = DataSegId(prodIds[0], i*sizeof(memData));
            dataSegs[i] = DataSeg(segIds[i], prodSize, memData);
        }

        prodId = prodIds[0];
        prodInfo = prodInfos[0];
        segId = segIds[0];
        dataSeg = dataSegs[0];
    }

public:
    void setState(const State state) {
        std::lock_guard<decltype(mutex)> lock{mutex};
        this->state = state;
        cond.notify_one();
    }

    void orState(const State state) {
        std::lock_guard<decltype(mutex)> guard{mutex};
        this->state = static_cast<State>(this->state | state);
        cond.notify_all();
    }

    void waitForState(const State nextState) {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (state != nextState)
            cond.wait(lock);
    }

    P2pSrvrInfo getSrvrInfo() override {
        return (std::this_thread::get_id() == subThreadId)
            ? subSrvrInfo
            : pubSrvrInfo;
    }

    void recv(const Tracker& tracker) override {
    }

    void start() {};
    void stop() {};
    void run() {};
    void halt() {};

    ProdIdSet subtract(ProdIdSet rhs) const override {
        return ProdIdSet{};
    }

    ProdIdSet getProdIds() const override {
        return ProdIdSet{};
    }

    void recvNotice(const P2pSrvrInfo& srvrInfo) override {
        orState(SRVR_INFO_NOTICE_RCVD);
    }

    // Subscriber-side
    bool recvNotice(const ProdId notice, SockAddr rmtAddr) override
    {
        LOG_TRACE("Entered");
        EXPECT_EQ(prodIds[prodNoticeCount++], notice);
        orState(PROD_NOTICE_RCVD);
        return true;
    }

    // Subscriber-side
    bool recvNotice(const DataSegId notice, SockAddr rmtAddr) override
    {
        LOG_TRACE("Entered");
        EXPECT_EQ(segIds[segNoticeCount++], notice);
        orState(SEG_NOTICE_RCVD);
        return true;
    }

    // Publisher-side
    ProdInfo getDatum(const ProdId request, const SockAddr rmtAddr) override
    {
        //LOG_DEBUG("prodId=%s", request.to_string().data());
        EXPECT_EQ(prodIds[prodRequestCount], request);
        orState(PROD_REQUEST_RCVD);
        static const ProdInfo invalid{};
        auto& prodInfo = (skipping && prodRequestCount == 0)
                ? invalid
                : prodInfos[prodRequestCount];
        ++prodRequestCount;
        return prodInfo;
    }

    // Publisher-side
    DataSeg getDatum(const DataSegId request, const SockAddr rmtAddr) override
    {
        //LOG_DEBUG("segId=%s", request.to_string().data());
        EXPECT_EQ(segIds[segRequestCount], request);
        orState(SEG_REQUEST_RCVD);
        static const DataSeg invalid{};
        auto& dataSeg = (skipping && segRequestCount == 0)
                ? invalid
                : dataSegs[segRequestCount];
        ++segRequestCount;
        return dataSeg;
    }

    // Subscriber-side
    void recvData(const ProdInfo data, SockAddr rmtAddr) override
    {
        LOG_TRACE("Entered");
        EXPECT_EQ((skipping) ? prodInfos[1] : prodInfos[prodDataCount++], data);
        orState(PROD_INFO_RCVD);
    }

    // Subscriber-side
    void recvData(const DataSeg actualDataSeg, SockAddr rmtAddr)
            override
    {
        LOG_TRACE("Entered");
        ASSERT_EQ(segSize, actualDataSeg.getSize());
        EXPECT_EQ(0, ::memcmp(dataSeg.getData(), actualDataSeg.getData(),
                segSize));
        orState(DATA_SEG_RCVD);
    }

    void missed(const ProdId prodId, SockAddr rmtAddr) override {
        static int i = 0;
        //LOG_DEBUG("i=%d, prodId=%s", i, prodId.to_string().data());
        ASSERT_EQ(prodIds[i++], prodId);
        orState(PROD_INFO_MISSED);
    }

    void missed(const DataSegId dataSegId, SockAddr rmtAddr) override {
        static int i = 0;
        //LOG_DEBUG("i=%d, dataSegId=%s", i, dataSegId.to_string().data());
        ASSERT_EQ(segIds[i++], dataSegId);
        orState(DATA_SEG_MISSED);
    }

    void notify(const ProdId prodInfo) {
    }

    void notify(const DataSegId dataSegId) {
    }

    void startPubPeer(PeerPtr& pubPeer)
    {
        try {
            auto pubPeerConnSrvr = PeerConnSrvr::create(pubAddr, 8);
            orState(LISTENING);

            // Blocks until subscribing peer connects
            pubPeer = Peer::create(*static_cast<Peer::PubMgr*>(this), pubPeerConnSrvr->accept(),
                    SysDuration(std::chrono::seconds(30)));

            auto rmtAddr = pubPeer->getRmtAddr().getInetAddr();
            EXPECT_EQ(pubAddr, rmtAddr);

            EXPECT_EQ(subSrvrInfo, pubPeer->getRmtSrvrInfo());
            orState(SRVR_INFO_RCVD);

            LOG_DEBUG("Starting publishing peer");
            pubPeerThread = Thread(&Peer::run, pubPeer.get());
        }
        catch (const std::exception& ex) {
            LOG_WARN(ex);
        }
    }

    void notify(PeerPtr pubPeer) {
        // Start an exchange
        pubPeer->notify(prodId);
        pubPeer->notify(segId);
    }
};

// Tests default construction
TEST_F(PeerTest, DefaultConstruction)
{
    PeerPtr peer{};
    EXPECT_FALSE(peer);
}

// Tests premature stopping
TEST_F(PeerTest, PrematureStop)
{
    // Create and execute publishing-peer on separate thread
    PeerPtr pubPeer{};
    std::thread srvrThread(&PeerTest::startPubPeer, this, std::ref(pubPeer));

    try {
        // Publishing peer is running

        waitForState(LISTENING);

        subThreadId = std::this_thread::get_id();
        auto subPeer = Peer::create(*this, pubAddr, SysDuration(std::chrono::seconds(30)));
        Thread subPeerThread(&Peer::run, subPeer.get());

        ASSERT_TRUE(srvrThread.joinable());
        srvrThread.join();

        subPeer->halt();
        subPeerThread.join();

        pubPeer->halt();
        pubPeerThread.join();
    } // `srvrThread` created
    catch (const std::exception& ex) {
        LOG_ERROR(ex);
        pubPeer->halt();
        pubPeerThread.join();
        if (srvrThread.joinable())
            srvrThread.join();
    }
}

#if 0
// Tests premature destruction
TEST_F(PeerTest, PrematureDtor)
{
    // Create and execute reception by publishing-peer on separate thread
    Peer pubPeer{};
    std::thread srvrThread(&PeerTest::startPubPeer, this, std::ref(pubPeer));

    try {
        waitForState(LISTENING);

        Peer subPeer(*this, pubAddr);
        ASSERT_TRUE(subPeer);
        ASSERT_TRUE(subPeer.start());

        ASSERT_TRUE(srvrThread.joinable());
        srvrThread.join();
    } // `srvrThread` created
    catch (const std::exception& ex) {
        LOG_ERROR(ex);
        if (srvrThread.joinable())
            srvrThread.join();
    }
}
#endif

// Tests data exchange
TEST_F(PeerTest, DataExchange)
{
    // Create and execute a publishing-peer on a separate thread
    PeerPtr pubPeer{};
    std::thread srvrThread(&PeerTest::startPubPeer, this, std::ref(pubPeer));

    try {
        waitForState(LISTENING);

        // Create and execute a subscribing-peer on a separate thread
        subThreadId = std::this_thread::get_id();
        auto subPeer = Peer::create(*this, pubAddr, SysDuration(std::chrono::seconds(30)));
        EXPECT_EQ(pubSrvrInfo, subPeer->getRmtSrvrInfo());
        orState(SRVR_INFO_RCVD);
        /*
         * If this program is executed in a "while" loop, then the following
         * will eventually cause the process to crash due to a segmentation
         * violation (SIGSEGV) because the subscribing peer will be unable to
         * establish a 3 socket connection with the publishing peer because a
         * `::connect()` call will have failed because it was unable to assign
         * the socket a local address using the O/S-chosen port number.
         * Apparently, there's a race condition for O/S-assigned port numbers in
         * a `::connect()` call for an unbound socket. Sheesh!
         */
        Thread subPeerThread(&Peer::run, subPeer.get());

        ASSERT_TRUE(srvrThread.joinable());
        srvrThread.join();
        // `pubPeer` is running

        // Start an exchange
        notify(pubPeer);

        // Wait for the exchange to complete
        auto done = static_cast<State>(
                LISTENING |
                SRVR_INFO_RCVD |
                PROD_NOTICE_RCVD |
                SEG_NOTICE_RCVD |
                PROD_REQUEST_RCVD |
                SEG_REQUEST_RCVD |
                PROD_INFO_RCVD |
                DATA_SEG_RCVD);
        waitForState(done);

        subPeer->halt();
        subPeerThread.join();

        pubPeer->halt();
        pubPeerThread.join();
    } // `srvrThread` created
    catch (const std::exception& ex) {
        LOG_ERROR(ex);
        pubPeer->halt();
        pubPeerThread.join();
        if (srvrThread.joinable())
            srvrThread.join();
    }
}

// Tests broken connection
TEST_F(PeerTest, BrokenConnection)
{
    // Create and execute reception by publishing peer on separate thread
    PeerPtr pubPeer{};
    std::thread srvrThread(&PeerTest::startPubPeer, this, std::ref(pubPeer));

    try {
        waitForState(LISTENING);

        {
            // Create and execute reception by subscribing peer on separate thread
            subThreadId = std::this_thread::get_id();
            auto subPeer = Peer::create(*this, pubAddr, SysDuration(std::chrono::seconds(30)));
            EXPECT_EQ(pubSrvrInfo, subPeer->getRmtSrvrInfo());
            orState(SRVR_INFO_RCVD);
            LOG_DEBUG("Starting subscribing peer");
            Thread subPeerThread(&Peer::run, subPeer.get());

            ASSERT_TRUE(srvrThread.joinable());
            srvrThread.join();
            // `pubPeer` is running

            LOG_DEBUG("Stopping subscribing peer");
            subPeer->halt();
            subPeerThread.join();
        } // `subPeer` destroyed

        // Try to send to subscribing peer
        LOG_DEBUG("Notifying subscribing peer");
        notify(pubPeer);
        // The broken connection should stop the publishing peer
        pubPeerThread.join();
    } // `srvrThread` running
    catch (const std::exception& ex) {
        LOG_ERROR(ex);
        pubPeer->halt();
        pubPeerThread.join();
        if (srvrThread.joinable())
            srvrThread.join();
    }
}

// Tests unsatisfied requests
TEST_F(PeerTest, UnsatisfiedRequests)
{
    skipping = true;

    // Create and execute reception by publishing peer on separate thread
    PeerPtr pubPeer{};
    std::thread srvrThread(&PeerTest::startPubPeer, this, std::ref(pubPeer));

    try {
        waitForState(LISTENING);

        {
            // Create and execute reception by subscribing peer on separate thread
            subThreadId = std::this_thread::get_id();
            auto subPeer = Peer::create(*this, pubAddr, SysDuration(std::chrono::seconds(30)));
            EXPECT_EQ(pubSrvrInfo, subPeer->getRmtSrvrInfo());
            orState(SRVR_INFO_RCVD);
            LOG_DEBUG("Starting subscribing peer");
            Thread subPeerThread(&Peer::run, subPeer.get());

            ASSERT_TRUE(srvrThread.joinable());
            srvrThread.join();
            // `pubPeer` is running

            // Start an exchange
            pubPeer->notify(prodIds[0]);
            pubPeer->notify(segIds[0]);
            pubPeer->notify(segIds[1]);
            pubPeer->notify(prodIds[1]);

            // Wait for the exchange to complete
            const auto done = static_cast<State>(
                LISTENING |
                SRVR_INFO_RCVD |
                PROD_NOTICE_RCVD |
                SEG_NOTICE_RCVD |
                PROD_REQUEST_RCVD |
                SEG_REQUEST_RCVD |
                PROD_INFO_RCVD |
                DATA_SEG_RCVD |
                PROD_INFO_MISSED |
                DATA_SEG_MISSED);
            waitForState(done);

            subPeer->halt();
            subPeerThread.join();

            pubPeer->halt();
            pubPeerThread.join();
        } // `subPeer` destroyed
    } // `srvrThread` running
    catch (const std::exception& ex) {
        LOG_ERROR(ex);
        pubPeer->halt();
        pubPeerThread.join();
        if (srvrThread.joinable())
            srvrThread.join();
    }
}

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
  log_setName(::basename(argv[0]));
  log_setLevel(LogLevel::INFO);

  std::set_terminate(&myTerminate);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
