#include "config.h"

#include "HycastProto.h"
#include "logging.h"
#include "P2pConn.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <signal.h>
#include <thread>

namespace {

/// The fixture for testing class `P2pConn`
class P2pTest : public ::testing::Test, public hycast::P2pNode
{
protected:
    typedef enum {
        INIT = 0,
        LISTENING = 0x1,
        PROD_NOTICE_RCVD = 0x4,
        SEG_NOTICE_RCVD = 0x8,
        PROD_REQUEST_RCVD = 0x10,
        SEG_REQUEST_RCVD = 0x20,
        PROD_INFO_RCVD = 0x40,
        SEG_RCVD = 0x80,
        DONE = LISTENING |
               PROD_NOTICE_RCVD |
               SEG_NOTICE_RCVD |
               PROD_REQUEST_RCVD |
               SEG_REQUEST_RCVD |
               PROD_INFO_RCVD |
               SEG_RCVD
    } State;
    State                   state;
    hycast::SockAddr        pubAddr;
    std::mutex              mutex;
    std::condition_variable cond;
    hycast::ProdIndex       prodIndex;
    hycast::ProdSize        prodSize;
    hycast::SegSize         segSize;
    hycast::ProdInfo        prodInfo;
    hycast::DataSegId       segId;
    char                    memData[hycast::DataSeg::CANON_DATASEG_SIZE];
    hycast::DataSeg         dataSeg;

    P2pTest()
        : state{INIT}
        , pubAddr{"localhost:38800"}
        , mutex{}
        , cond{}
        , prodIndex{1}
        , prodSize{1000000}
        , segSize{sizeof(memData)}
        , prodInfo{prodIndex, "product", prodSize}
        , segId(prodIndex, sizeof(memData)) // Second data-segment
        , memData{}
        , dataSeg{segId, prodSize, memData}
    {
        ::memset(memData, 0xbd, segSize);
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

    // Publisher-side
    bool isPublisher() const override {
        LOG_TRACE;
        return true;
    }

    // Publisher-side
    bool isPathToPub() const override {
        LOG_TRACE;
        return true;
    }

    // Both sides
    void recvNotice(const hycast::PubPath notice, hycast::Peer peer)
            override
    {
        LOG_TRACE;
    }

    // Subscriber-side
    void recvNotice(const hycast::ProdIndex notice, hycast::Peer peer)
            override
    {
        LOG_TRACE;
        EXPECT_EQ(notice, prodIndex);
        orState(PROD_NOTICE_RCVD);
        peer.request(notice);
    }

    // Subscriber-side
    void recvNotice(const hycast::DataSegId& notice, hycast::Peer peer)
            override
    {
        LOG_TRACE;
        EXPECT_EQ(segId, notice);
        orState(SEG_NOTICE_RCVD);
        peer.request(notice);
    }

    // Publisher-side
    void recvRequest(const hycast::ProdIndex request, hycast::Peer peer)
            override
    {
        LOG_TRACE;
        EXPECT_TRUE(prodIndex == request);
        orState(PROD_REQUEST_RCVD);
        peer.send(prodInfo);
    }

    // Publisher-side
    void recvRequest(const hycast::DataSegId& request, hycast::Peer peer)
            override
    {
        LOG_TRACE;
        EXPECT_EQ(segId, request);
        orState(SEG_REQUEST_RCVD);
        peer.send(dataSeg);
    }

    // Subscriber-side
    void recvData(const hycast::ProdInfo& data, hycast::Peer peer) override
    {
        LOG_TRACE;
        EXPECT_EQ(prodInfo, data);
        orState(PROD_INFO_RCVD);
    }

    // Subscriber-side
    void recvData(const hycast::DataSeg& actualDataSeg, hycast::Peer peer)
            override
    {
        LOG_TRACE;
        ASSERT_EQ(segSize, actualDataSeg.size());
        EXPECT_EQ(0, ::memcmp(dataSeg.data(), actualDataSeg.data(), segSize));
        orState(SEG_RCVD);
    }

    void died(hycast::Peer peer) {
        LOG_ERROR("Peer %s died", peer.to_string().data());
    }
    void reassigned(const hycast::ProdIndex  notice,
                    hycast::Peer             peer) {}
    void reassigned(const hycast::DataSegId& notice,
                    hycast::Peer             peer) {}

    void startPubPeer(hycast::Peer& pubPeer)
    {
        hycast::TcpSrvrSock srvrSock(pubAddr);
        orState(LISTENING);

        auto                pubSock = srvrSock.accept();
        auto                rmtAddr = pubSock.getRmtAddr().getInetAddr();
        hycast::InetAddr    localhost("127.0.0.1");

        EXPECT_EQ(localhost, rmtAddr);

        pubPeer = hycast::Peer{pubSock, *this};
        ASSERT_TRUE(pubPeer);
        pubPeer.start();
    }

    void notify(hycast::Peer pubPeer) {
        // Start an exchange
        pubPeer.notify(prodIndex);
        pubPeer.notify(segId);
    }

    void loopNotify(hycast::Peer pubPeer) {
        for (;;)
            notify(pubPeer);
    }
};

// Tests default construction
TEST_F(P2pTest, DefaultConstruction)
{
    hycast::Peer peer{};
    EXPECT_FALSE(peer);
}

// Tests data exchange
TEST_F(P2pTest, DataExchange)
{
    // Create and execute reception by publishing peer on separate thread
    hycast::Peer pubPeer{};
    std::thread srvrThread(&P2pTest::startPubPeer, this, std::ref(pubPeer));

    waitForState(LISTENING);

    // Create and execute reception by subscribing peer on separate thread
    hycast::Peer subPeer(pubAddr, *this);
    ASSERT_TRUE(subPeer);
    subPeer.start();

    ASSERT_TRUE(srvrThread.joinable());
    srvrThread.join(); // `pubPeer` is running upon return

    // Start an exchange
    notify(pubPeer);

    // Wait for the exchange to complete
    waitForState(DONE);
    subPeer.stop();
    pubPeer.stop();
}

// Tests broken connection
TEST_F(P2pTest, BrokenConnection)
{
    // Create and execute reception by publishing peer on separate thread
    hycast::Peer pubPeer{};
    std::thread srvrThread(&P2pTest::startPubPeer, this, std::ref(pubPeer));

    waitForState(LISTENING);

    {
        // Create and execute reception by subscribing peer on separate thread
        hycast::Peer subPeer{pubAddr, *this};
        ASSERT_TRUE(subPeer);
        subPeer.start();
        subPeer.stop();
    } // `subPeer` destroyed

    ASSERT_TRUE(srvrThread.joinable());
    srvrThread.join(); // `pubPeer` is running upon return

    // Try to send to subscribing peer
    ASSERT_THROW(loopNotify(pubPeer), hycast::EofError);

    pubPeer.stop();
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
  hycast::log_setName(::basename(argv[0]));
  //hycast::log_setLevel(hycast::LogLevel::TRACE);

  std::set_terminate(&myTerminate);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
