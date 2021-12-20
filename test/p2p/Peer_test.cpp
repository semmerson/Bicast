#include "config.h"

#include "HycastProto.h"
#include "logging.h"
#include "Peer.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <signal.h>
#include <thread>

namespace {

using namespace hycast;

/// The fixture for testing class `Peer`
class PeerTest : public ::testing::Test, public SubP2pMgr
{
protected:
    typedef enum {
        INIT = 0,
        LISTENING             =   0x1,
        PEER_SRVR_ADDRS_RCVD  =   0x2,
        PROD_NOTICE_RCVD      =   0x4,
        SEG_NOTICE_RCVD       =   0x8,
        PROD_REQUEST_RCVD     =  0x10,
        SEG_REQUEST_RCVD      =  0x20,
        PROD_INFO_RCVD        =  0x40,
        DATA_SEG_RCVD         =  0x80,
        PROD_INFO_MISSED      = 0x100,
        DATA_SEG_MISSED       = 0x200
    } State;
    State             state;
    SockAddr          pubAddr;
    std::mutex        mutex;
    Cond              cond;
    ProdIndex         prodIndexes[2];
    ProdIndex         prodIndex;
    ProdSize          prodSize;
    SegSize           segSize;
    ProdInfo          prodInfos[2];
    ProdInfo          prodInfo;
    DataSegId         segIds[2];
    DataSegId         segId;
    char              memData[DataSeg::CANON_DATASEG_SIZE];
    DataSeg           dataSegs[2];
    DataSeg           dataSeg;
    std::atomic<bool> skipping;
    int               prodNoticeCount;
    int               segNoticeCount;
    int               prodRequestCount;
    int               segRequestCount;
    int               prodDataCount;
    int               segDataCount;

    PeerTest()
        : state(INIT)
        , pubAddr("localhost:38800")
        , mutex()
        , cond()
        , prodIndexes()
        , prodIndex()
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
    {
        String prodNames[2] = {"product1", "product2"};

        ::memset(memData, 0xbd, segSize);

        for (int i = 0; i < 2; ++i) {
            prodIndexes[i] = ProdIndex(i);
            prodInfos[i] = ProdInfo(prodIndexes[i], prodNames[i], prodSize);
            segIds[i] = DataSegId(prodIndexes[0], i*sizeof(memData));
            dataSegs[i] = DataSeg(segIds[i], prodSize, memData);
        }

        prodIndex = prodIndexes[0];
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

    void orState(const State state)
    {
        std::lock_guard<decltype(mutex)> guard{mutex};
        this->state = static_cast<State>(this->state | state);
        cond.notify_all();
    }

    void waitForState(const State nextState)
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (state != nextState)
            cond.wait(lock);
    }

    // Both sides
    void waitForSrvrPeer() override {}

    SockAddr getPeerSrvrAddr() const override {
        return SockAddr();
    }

    bool shouldNotify(
            Peer      peer,
            ProdIndex prodIndex) override {
        return true;
    }

    bool shouldNotify(
            Peer      peer,
            DataSegId segId) override {
        return true;
    }

    // Subscriber-side
    bool recvNotice(const ProdIndex notice, Peer peer) override
    {
        LOG_TRACE;
        EXPECT_EQ(prodIndexes[prodNoticeCount++], notice);
        orState(PROD_NOTICE_RCVD);
        return true;
    }

    // Subscriber-side
    bool recvNotice(const DataSegId notice, Peer peer) override
    {
        LOG_TRACE;
        EXPECT_EQ(segIds[segNoticeCount++], notice);
        orState(SEG_NOTICE_RCVD);
        return true;
    }

    // Publisher-side
    ProdInfo recvRequest(const ProdIndex request,
                         Peer            peer) override
    {
        LOG_TRACE;
        EXPECT_EQ(prodIndexes[prodRequestCount], request);
        orState(PROD_REQUEST_RCVD);
        auto prodInfo = (skipping && prodRequestCount == 0)
                ? ProdInfo{}
                : prodInfos[prodRequestCount];
        ++prodRequestCount;
        return prodInfo;
    }

    // Publisher-side
    DataSeg recvRequest(const DataSegId request,
                        Peer            peer) override
    {
        LOG_TRACE;
        EXPECT_EQ(segIds[segRequestCount], request);
        orState(SEG_REQUEST_RCVD);
        auto dataSeg = (skipping && segRequestCount == 0)
                ? DataSeg{}
                : dataSegs[segRequestCount];
        ++segRequestCount;
        return dataSeg;
    }

    // Subscriber-side
    void recvData(const Tracker tracker, Peer peer) override
    {
        LOG_TRACE;
        orState(PEER_SRVR_ADDRS_RCVD);
    }

    // Subscriber-side
    void recvData(const ProdInfo data, Peer peer) override
    {
        LOG_TRACE;
        EXPECT_EQ((skipping) ? prodInfos[1] : prodInfos[prodDataCount++], data);
        orState(PROD_INFO_RCVD);
    }

    // Subscriber-side
    void recvData(const DataSeg actualDataSeg, Peer peer)
            override
    {
        LOG_TRACE;
        ASSERT_EQ(segSize, actualDataSeg.getSize());
        EXPECT_EQ(0, ::memcmp(dataSeg.getData(), actualDataSeg.getData(),
                segSize));
        orState(DATA_SEG_RCVD);
    }

    void missed(const ProdIndex prodIndex, Peer peer) override {
        ASSERT_EQ(prodIndexes[0], prodIndex);
        orState(PROD_INFO_MISSED);
    }

    void missed(const DataSegId dataSegId, Peer peer) override {
        ASSERT_EQ(segIds[0], dataSegId);
        orState(DATA_SEG_MISSED);
    }

    void notify(const ProdIndex prodInfo) {
    }

    void notify(const DataSegId dataSegId) {
    }

    void lostConnection(Peer peer) override {
        LOG_INFO("Lost connection with peer %s ", peer.to_string().data());
    }

    void startPubPeer(Peer& pubPeer)
    {
        PubPeerSrvr peerSrvr{*this, pubAddr};
        orState(LISTENING);

        pubPeer = peerSrvr.accept();
        ASSERT_TRUE(pubPeer);

        auto rmtAddr = pubPeer.getRmtAddr().getInetAddr();
        InetAddr localhost("127.0.0.1");
        EXPECT_EQ(localhost, rmtAddr);

        LOG_DEBUG("Starting publishing peer");
        ASSERT_TRUE(pubPeer.start());
    }

    bool notify(Peer& pubPeer) {
        // Start an exchange
        return pubPeer.notify(prodIndex) &&
                pubPeer.notify(segId);
    }

    bool loopNotify(Peer pubPeer) {
        while (notify(pubPeer))
            ::pthread_yield();
        return false;
    }
};

// Tests default construction
TEST_F(PeerTest, DefaultConstruction)
{
    Peer peer{};
    EXPECT_FALSE(peer);
}

// Tests premature stopping
TEST_F(PeerTest, PrematureStop)
{
    // Create and execute reception by publishing-peer on separate thread
    Peer pubPeer{};
    std::thread srvrThread(&PeerTest::startPubPeer, this, std::ref(pubPeer));

    try {
        waitForState(LISTENING);

        SubPeer subPeer(*this, pubAddr);
        ASSERT_TRUE(subPeer);
        ASSERT_TRUE(subPeer.start());

        ASSERT_TRUE(srvrThread.joinable());
        srvrThread.join();

        subPeer.stop();

        pubPeer.stop();
    } // `srvrThread` created
    catch (const std::exception& ex) {
        LOG_ERROR(ex);
        pubPeer.stop();
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
    // Create and execute reception by publishing-peer on separate thread
    Peer pubPeer{};
    std::thread srvrThread(&PeerTest::startPubPeer, this, std::ref(pubPeer));

    try {
        waitForState(LISTENING);

        // Create and execute reception by subscribing-peer on separate thread
        SubPeer subPeer(*this, pubAddr);
        ASSERT_TRUE(subPeer);
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
        ASSERT_TRUE(subPeer.start());

        ASSERT_TRUE(srvrThread.joinable());
        srvrThread.join();
        // `pubPeer` is running

        // Start an exchange
        ASSERT_TRUE(notify(pubPeer));

        // Wait for the exchange to complete
        auto done = static_cast<State>(
               LISTENING |
               PROD_NOTICE_RCVD |
               SEG_NOTICE_RCVD |
               PROD_REQUEST_RCVD |
               SEG_REQUEST_RCVD |
               PROD_INFO_RCVD |
               DATA_SEG_RCVD);
        waitForState(done);
        subPeer.stop();
        pubPeer.stop();
    } // `srvrThread` created
    catch (const std::exception& ex) {
        LOG_ERROR(ex);
        pubPeer.stop();
        if (srvrThread.joinable())
            srvrThread.join();
    }
}

// Tests broken connection
TEST_F(PeerTest, BrokenConnection)
{
    // Create and execute reception by publishing peer on separate thread
    Peer pubPeer{};
    std::thread srvrThread(&PeerTest::startPubPeer, this, std::ref(pubPeer));

    try {
        waitForState(LISTENING);

        {
            // Create and execute reception by subscribing peer on separate thread
            SubPeer subPeer{*this, pubAddr};
            ASSERT_TRUE(subPeer);
            LOG_DEBUG("Starting subscribing peer");
            /*
             * If this program is executed in a "while" loop, then the following
             * will eventually cause the process to crash due to a segmentation
             * violation (SIGSEGV) because the subscribing peer will be unable
             * to establish a 3 socket connection with the publishing peer
             * because a `::connect()` call will have failed because it was
             * unable to assign the socket a local address using the O/S-chosen
             * port number. Apparently, there's a race condition for
             * O/S-assigned port numbers in a `::connect()` call for an unbound
             * socket. Sheesh!
             */
            ASSERT_TRUE(subPeer.start());

            ASSERT_TRUE(srvrThread.joinable());
            srvrThread.join();
            // `pubPeer` is running

            LOG_DEBUG("Stopping subscribing peer");
            subPeer.stop();
        } // `subPeer` destroyed

        // Try to send to subscribing peer
        LOG_DEBUG("Notifying subscribing peer");
        ASSERT_FALSE(loopNotify(pubPeer));
        pubPeer.stop();
    } // `srvrThread` running
    catch (const std::exception& ex) {
        LOG_ERROR(ex);
        pubPeer.stop();
        if (srvrThread.joinable())
            srvrThread.join();
    }
}

// Tests unsatisfied requests
TEST_F(PeerTest, UnsatisfiedRequests)
{
    skipping = true;

    // Create and execute reception by publishing peer on separate thread
    Peer pubPeer{};
    std::thread srvrThread(&PeerTest::startPubPeer, this, std::ref(pubPeer));

    try {
        waitForState(LISTENING);

        {
            // Create and execute reception by subscribing peer on separate thread
            SubPeer subPeer{*this, pubAddr};
            ASSERT_TRUE(subPeer);
            LOG_DEBUG("Starting subscribing peer");
            ASSERT_TRUE(subPeer.start());

            ASSERT_TRUE(srvrThread.joinable());
            srvrThread.join();
            // `pubPeer` is running

            // Start an exchange
            ASSERT_TRUE(pubPeer.notify(prodIndexes[0]));
            ASSERT_TRUE(pubPeer.notify(segIds[0]));
            ASSERT_TRUE(pubPeer.notify(segIds[1]));
            ASSERT_TRUE(pubPeer.notify(prodIndexes[1]));

            // Wait for the exchange to complete
            const auto done = static_cast<State>(
                LISTENING |
                PROD_NOTICE_RCVD |
                SEG_NOTICE_RCVD |
                PROD_REQUEST_RCVD |
                SEG_REQUEST_RCVD |
                PROD_INFO_RCVD |
                DATA_SEG_RCVD |
                PROD_INFO_MISSED |
                DATA_SEG_MISSED);
            waitForState(done);
            subPeer.stop();
            pubPeer.stop();
        } // `subPeer` destroyed
    } // `srvrThread` running
    catch (const std::exception& ex) {
        LOG_ERROR(ex);
        pubPeer.stop();
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
  log_setLevel(LogLevel::DEBUG);

  std::set_terminate(&myTerminate);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
