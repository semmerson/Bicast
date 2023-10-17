#include "config.h"

#include "PeerSet.h"
#include "logging.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <P2pMgr.h>
#include <mutex>
#include <thread>

namespace {

using namespace bicast;

/// The fixture for testing class `PeerSet`
class PeerSetTest : public ::testing::Test, public SubP2pMgr
{
protected:
    typedef enum {
        INIT              =    0,
        LISTENING         =  0x1,
        PROD_NOTICE_RCVD  =  0x2,
        SEG_NOTICE_RCVD   =  0x4,
        PROD_REQUEST_RCVD = 0x08,
        SEG_REQUEST_RCVD  = 0x10,
        PROD_INFO_RCVD    = 0x20,
        SEG_RCVD          = 0x40,
        DONE = LISTENING         |
               PROD_NOTICE_RCVD  |
               SEG_NOTICE_RCVD   |
               PROD_REQUEST_RCVD |
               SEG_REQUEST_RCVD  |
               PROD_INFO_RCVD    |
               SEG_RCVD
    } State;
    State                   state;
    SockAddr                pubAddr;
    std::mutex              mutex;
    std::condition_variable cond;
    ProdId                  prodId;
    ProdSize                prodSize;
    SegSize                 segSize;
    ProdInfo                prodInfo;
    DataSegId               segId;
    char                    memData[900];
    DataSeg                 dataSeg;
    int                     pubPathNoticeCount;
    int                     prodInfoNoticeCount;
    int                     dataSegNoticeCount;
    int                     prodInfoRequestCount;
    int                     dataSegRequestCount;
    int                     prodInfoCount;
    int                     dataSegCount;

    static const int        NUM_SUBSCRIBERS = 1;

    PeerSetTest()
        : state{INIT}
        , pubAddr{"localhost:38800"}
        , mutex{}
        , cond{}
        , prodId{1}
        , prodSize{1000000}
        , segSize{sizeof(memData)}
        , prodInfo{prodId, "product", prodSize}
        , segId(prodId, sizeof(memData)) // Second data-segment
        , memData{}
        , dataSeg{segId, prodSize, memData}
        , pubPathNoticeCount(0)
        , prodInfoNoticeCount(0)
        , dataSegNoticeCount(0)
        , prodInfoRequestCount(0)
        , dataSegRequestCount(0)
        , prodInfoCount(0)
        , dataSegCount(0)
    {
        DataSeg::setMaxSegSize(sizeof(memData));
        ::memset(memData, 0xbd, segSize);
    }

public:
    void orState(const State state)
    {
        this->state = static_cast<State>(this->state | state);
        cond.notify_all();
    }

    void waitForState(const State nextState)
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (state != nextState)
            cond.wait(lock);
    }

    // Publisher-side
    bool isPublisher() const {
        LOG_TRACE;
        return true;
    }

    // Both sides
    void waitForSrvrPeer() override {}

    // Publisher-side
    bool isPathToPub() const {
        LOG_TRACE;
        return true;
    }

    SockAddr getPeerSrvrAddr() const override {
        return SockAddr();
    }

    bool shouldNotify(
            Peer   peer,
            ProdId prodId) override {
        return true;
    }

    bool shouldNotify(
            Peer      peer,
            DataSegId segId) override {
        return true;
    }

    // Subscriber-side
    bool recvNotice(const ProdId notice, Peer peer) override {
        LOG_TRACE;
        EXPECT_EQ(notice, prodId);
        {
            std::lock_guard<std::mutex> guard{mutex};
            if (++prodInfoNoticeCount == NUM_SUBSCRIBERS)
                orState(PROD_NOTICE_RCVD);
        }
        return true;
    }

    // Subscriber-side
    bool recvNotice(const DataSegId notice, Peer peer) override {
        LOG_TRACE;
        EXPECT_EQ(segId, notice);
        {
            std::lock_guard<std::mutex> guard{mutex};
            if (++dataSegNoticeCount == NUM_SUBSCRIBERS)
                orState(SEG_NOTICE_RCVD);
        }
        return true;
    }

    // Publisher-side
    ProdInfo recvRequest(const ProdId request, Peer peer) override {
        LOG_TRACE;
        EXPECT_TRUE(prodId == request);
        {
            std::lock_guard<std::mutex> guard{mutex};
            if (++prodInfoRequestCount == NUM_SUBSCRIBERS)
                orState(PROD_REQUEST_RCVD);
        }
        return prodInfo;
    }

    // Publisher-side
    DataSeg recvRequest(const DataSegId request, Peer peer) override {
        LOG_TRACE;
        EXPECT_EQ(segId, request);
        {
            std::lock_guard<std::mutex> guard{mutex};
            if (++dataSegRequestCount == NUM_SUBSCRIBERS)
                orState(SEG_REQUEST_RCVD);
        }
        return dataSeg;
    }

    void missed(const ProdId prodId, Peer peer) override {
    }

    void missed(const DataSegId dataSegId, Peer peer) override {
    }

    void notify(const ProdId prodInfo) override {
    }

    void notify(const DataSegId dataSegId) override {
    }

    // Subscriber-side
    void recvData(const Tracker tracker, Peer peer) override {
        // TODO
    }

    // Subscriber-side
    void recvData(const ProdInfo data, Peer peer) override {
        LOG_TRACE;
        EXPECT_EQ(prodInfo, data);
        std::lock_guard<std::mutex> guard{mutex};
        if (++prodInfoCount == NUM_SUBSCRIBERS)
            orState(PROD_INFO_RCVD);
    }

    // Subscriber-side
    void recvData(const DataSeg actualDataSeg, Peer peer) override {
        LOG_TRACE;
        ASSERT_EQ(segSize, actualDataSeg.getSize());
        EXPECT_EQ(0, ::memcmp(dataSeg.getData(), actualDataSeg.getData(),
                segSize));
        std::lock_guard<std::mutex> guard{mutex};
        if (++dataSegCount == NUM_SUBSCRIBERS)
            orState(SEG_RCVD);
    }

    void lostConnection(Peer peer) override {
        LOG_INFO("Lost connection with peer ", peer.to_string().data());
    }

    void startPublisher(PeerSet pubPeerSet)
    {
        PubP2pSrvr peerSrvr{*this, pubAddr};
        orState(LISTENING);

        for (int i = 0; i < NUM_SUBSCRIBERS; ++i) {
            auto             pubPeer = peerSrvr.accept();
            auto             rmtAddr = pubPeer.getRmtAddr().getInetAddr();
            InetAddr localhost("127.0.0.1");
            EXPECT_EQ(localhost, rmtAddr);

            pubPeer.start();              // Starts reading
            pubPeerSet.insert(pubPeer);   // Ready to notify
            ASSERT_EQ(i+1, pubPeerSet.size());
        }
    }
};

// Tests default construction
TEST_F(PeerSetTest, DefaultConstruction)
{
    PeerSet peerSet{};
}

// Tests data exchange
TEST_F(PeerSetTest, DataExchange)
{
    try {
        // Create and execute publisher
        PeerSet pubPeerSet{};
        std::thread     srvrThread{&PeerSetTest::startPublisher, this,
                pubPeerSet};

        try {
            waitForState(LISTENING);

            /*
             * Create and execute reception by subscribing peers on separate
             * threads
             */
            PeerSet subPeerSet{};
            for (int i = 0; i < NUM_SUBSCRIBERS; ++i) {
                SubPeer subPeer(*this, pubAddr);
                subPeer.start(); // Starts reading
                subPeerSet.insert(subPeer);
                ASSERT_EQ(i+1, subPeerSet.size());
            }

            ASSERT_TRUE(srvrThread.joinable());
            srvrThread.join();

            // Start an exchange
            pubPeerSet.notify(prodId);
            pubPeerSet.notify(segId);

            // Wait for the exchange to complete
            waitForState(DONE);
        }
        catch (const std::exception& ex) {
            if (srvrThread.joinable())
                srvrThread.join();
            throw;
        }
    }
    catch (const std::exception& ex) {
        LOG_ERROR(ex);
    }
}

}  // namespace

static void myTerminate()
{
    if (std::current_exception()) {
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
    else {
        LOG_FATAL("terminate() called without an active exception");
    }
    abort();
}

int main(int argc, char **argv) {
  log_setName(::basename(argv[0]));
  //log_setLevel(LogLevel::TRACE);

  std::set_terminate(&myTerminate);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
