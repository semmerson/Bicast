#include "config.h"

#include "HycastProto.h"
#include "logging.h"
#include "Peer.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `Peer`
class PeerTest : public ::testing::Test, public hycast::Node
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
    hycast::Peer            pubPeer;
    hycast::Peer            subPeer;
    std::mutex              mutex;
    std::condition_variable cond;
    hycast::ProdIndex       prodIndex;
    hycast::ProdSize        prodSize;
    hycast::SegSize         segSize;
    hycast::ProdInfo        prodInfo;
    hycast::DataSegId       segId;
    char                    memData[hycast::DataSeg::CANON_DATASEG_SIZE];
    hycast::DataSeg         dataSeg;

    PeerTest()
        : state{INIT}
        , pubAddr{"localhost:38800"}
        , pubPeer()
        , subPeer()
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
    void recvNotice(const hycast::PubPathNotice& notice, hycast::Peer& peer)
            override
    {
        LOG_TRACE;
    }

    // Subscriber-side
    void recvNotice(const hycast::ProdIndex& notice, hycast::Peer& peer)
            override
    {
        LOG_TRACE;
        EXPECT_EQ(notice, prodIndex);
        orState(PROD_NOTICE_RCVD);
        peer.request(notice);
    }

    // Subscriber-side
    void recvNotice(const hycast::DataSegId& notice, hycast::Peer& peer)
            override
    {
        LOG_TRACE;
        EXPECT_EQ(segId, notice);
        orState(SEG_NOTICE_RCVD);
        peer.request(notice);
    }

    // Publisher-side
    void recvRequest(const hycast::ProdIndex& request, hycast::Peer& peer)
            override
    {
        LOG_TRACE;
        EXPECT_TRUE(prodIndex == request);
        orState(PROD_REQUEST_RCVD);
        peer.send(prodInfo);
    }

    // Publisher-side
    void recvRequest(const hycast::DataSegId& request, hycast::Peer& peer)
            override
    {
        LOG_TRACE;
        EXPECT_EQ(segId, request);
        orState(SEG_REQUEST_RCVD);
        peer.send(dataSeg);
    }

    // Subscriber-side
    void recvData(const hycast::ProdInfo& data, hycast::Peer& peer) override
    {
        LOG_TRACE;
        EXPECT_EQ(prodInfo, data);
        orState(PROD_INFO_RCVD);
    }

    // Subscriber-side
    void recvData(const hycast::DataSeg& data, hycast::Peer& peer) override
    {
        LOG_TRACE;
        ASSERT_EQ(segSize, data.size());
        EXPECT_EQ(0, ::memcmp(dataSeg.data, data.data, segSize));
        orState(SEG_RCVD);
    }

    void startPubPeer()
    {
        hycast::TcpSrvrSock srvrSock(pubAddr);
        orState(LISTENING);

        auto                pubSock = srvrSock.accept();
        auto                rmtAddr = pubSock.getRmtAddr().getInetAddr();
        hycast::InetAddr    localhost("127.0.0.1");

        EXPECT_EQ(localhost, rmtAddr);

        hycast::Peer peer{pubSock, *this};
        pubPeer = peer;
        ASSERT_TRUE(pubPeer);
        pubPeer.receive();
    }
};

// Tests default construction
TEST_F(PeerTest, DefaultConstruction)
{
    hycast::Peer peer{};
    EXPECT_FALSE(peer);
}

// Tests data exchange
TEST_F(PeerTest, DataExchange)
{
    // Create and execute reception by publishing peer on separate thread
    std::thread srvrThread(&PeerTest::startPubPeer, this);

    waitForState(LISTENING);

    // Create and execute reception by subscribing peer on separate thread
    subPeer = hycast::Peer(pubAddr, *this);
    ASSERT_TRUE(subPeer);

    ASSERT_TRUE(srvrThread.joinable());
    srvrThread.join(); // `pubPeer` is running upon return

    subPeer.receive();

    // Start an exchange
    pubPeer.notify(prodIndex);
    pubPeer.notify(segId);

    // Wait for the exchange to complete
    waitForState(DONE);
}

}  // namespace

static void myTerminate()
{
    LOG_FATAL("terminate() called %s an active exception",
            std::current_exception() ? "with" : "without");
    abort();
}

int main(int argc, char **argv) {
  hycast::log_setName(::basename(argv[0]));
  hycast::log_setLevel(hycast::LogLevel::TRACE);

  std::set_terminate(&myTerminate);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
