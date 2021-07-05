#include "config.h"

#include "error.h"
#include <condition_variable>
#include <gtest/gtest.h>
#include <main/p2p-old/Peer.h>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `Peer`
class PeerTest : public ::testing::Test, public hycast::XcvrPeerMgr
{
protected:
    typedef enum {
        INIT = 0,
        LISTENING = 0x1,
        CONNECTED = 0x2,
        PROD_NOTICE_RCVD = 0x4,
        SEG_NOTICE_RCVD = 0x8,
        PROD_REQUEST_RCVD = 0x10,
        SEG_REQUEST_RCVD = 0x20,
        PROD_INFO_RCVD = 0x40,
        SEG_RCVD = 0x80,
        DONE = CONNECTED |
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
    hycast::SegId           segId;
    hycast::SegInfo         segInfo;
    char                    memData[1000];
    hycast::MemSeg          memSeg;
    hycast::Peer            pubPeer;

    PeerTest()
        : state{INIT}
        , pubAddr{"localhost:38800"}
        , mutex{}
        , cond{}
        , prodIndex{1}
        , prodSize{1000000}
        , segSize{sizeof(memData)}
        , prodInfo{prodIndex, prodSize, "product"}
        , segId(prodIndex, segSize)
        , segInfo(segId, prodSize, segSize)
        , memData{}
        , memSeg{segInfo, memData}
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

    void pathToPub(const hycast::SockAddr& rmtAddr)
    {}

    void noPathToPub(const hycast::SockAddr& rmtAddr)
    {}

    // Receiver-side
    bool shouldRequest(
            const hycast::SockAddr& rmtAddr,
            const hycast::ProdIndex actual)
    {
        EXPECT_TRUE(prodIndex == actual);
        orState(PROD_NOTICE_RCVD);

        return true;
    }

    // Receiver-side
    bool shouldRequest(
            const hycast::SockAddr& rmtAddr,
            const hycast::SegId&    actual)
    {
        EXPECT_EQ(segId, actual);
        orState(SEG_NOTICE_RCVD);

        return true;
    }

    // Sender-side
    hycast::ProdInfo getProdInfo(
            const hycast::SockAddr& remote,
            const hycast::ProdIndex actual)
    {
        EXPECT_TRUE(prodIndex == actual);
        orState(PROD_REQUEST_RCVD);
        return prodInfo;
    }

    // Sender-side
    hycast::MemSeg getMemSeg(
            const hycast::SockAddr& remote,
            const hycast::SegId&    actual)
    {
        EXPECT_EQ(segId, actual);
        orState(SEG_REQUEST_RCVD);
        return memSeg;
    }

    // Receiver-side
    bool hereIs(
            const hycast::SockAddr& rmtAddr,
            const hycast::ProdInfo& actual)
    {
        EXPECT_EQ(prodInfo, actual);
        orState(PROD_INFO_RCVD);

        return true;
    }

    // Receiver-side
    bool hereIs(
            const hycast::SockAddr& rmtAddr,
            hycast::TcpSeg&         actual)
    {
        const hycast::SegSize size = actual.getSegInfo().getSegSize();
        EXPECT_EQ(segSize, size);

        char buf[size];
        actual.getData(buf);

        EXPECT_EQ(0, ::memcmp(memSeg.data(), buf, segSize));

        orState(SEG_RCVD);

        return true;
    }

    void runPublisher()
    {
        try {
            hycast::TcpSrvrSock srvrSock(pubAddr);

            setState(LISTENING);

            hycast::TcpSock pubSock(srvrSock.accept());
            pubPeer = hycast::Peer(pubSock, *this);

            auto             rmtAddr = pubPeer.getRmtAddr().getInetAddr();
            hycast::InetAddr localhost("127.0.0.1");
            EXPECT_EQ(localhost, rmtAddr);

            setState(CONNECTED);

            pubPeer();
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Logging exception");
            hycast::log_error(ex);
        }
        catch (...) {
            LOG_NOTE("Caught ...");
        }
    }
};

// Tests default construction
TEST_F(PeerTest, DefaultConstruction)
{
    hycast::Peer peer();
}

// Tests data exchange
TEST_F(PeerTest, DataExchange)
{
    // Start the publisher
    std::thread pubThread(&PeerTest::runPublisher, this);

    try {
        waitForState(LISTENING);

        // Start the subscriber. Potentially slow.
        hycast::Peer     subPeer(pubAddr,
                hycast::NodeType::NO_PATH_TO_PUBLISHER, *this);
        std::thread      subThread(subPeer);

        try {
            waitForState(CONNECTED);

            // Start an exchange
            pubPeer.notify(prodIndex);
            pubPeer.notify(segId);

            // Wait for the exchange to complete
            waitForState(DONE);

            // `subPeer()` returns & `subThread` terminates
            subPeer.halt();
            subThread.join();
        }
        catch (const std::exception& ex) {
            hycast::log_fatal(ex);
            subPeer.halt();
            subThread.join();
            throw;
        }
        catch (...) {
            LOG_FATAL("Thread cancellation?");
            subThread.join();
            throw;
        } // `subThread` active

        pubThread.join();
    } // `pubThread` active
    catch (const std::exception& ex) {
        hycast::log_fatal(ex);
        pubThread.join();
        throw;
    }
    catch (...) {
        LOG_FATAL("Thread cancellation?");
        pubThread.join();
        throw;
    }
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
