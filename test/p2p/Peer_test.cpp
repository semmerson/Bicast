#include "config.h"

#include "error.h"
#include "SockAddr.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <Peer.h>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `Peer`
class PeerTest : public ::testing::Test, public hycast::PeerObs
{
protected:
    typedef enum {
        INIT = 0,
        LISTENING = 1,
        PROD_NOTICE_RCVD = 2,
        SEG_NOTICE_RCVD = 4,
        PROD_REQUEST_RCVD = 8,
        SEG_REQUEST_RCVD = 16,
        PROD_INFO_RCVD = 32,
        SEG_RCVD = 64,
        DONE = LISTENING |
               PROD_NOTICE_RCVD |
               SEG_NOTICE_RCVD |
               PROD_REQUEST_RCVD |
               SEG_REQUEST_RCVD |
               PROD_INFO_RCVD |
               SEG_RCVD
    } State;
    State                   state;
    hycast::SockAddr        srvrAddr;
    hycast::PortPool        portPool;
    std::mutex              mutex;
    std::condition_variable cond;
    hycast::ProdIndex          prodId;
    hycast::ProdSize        prodSize;
    hycast::SegSize         segSize;
    hycast::ProdInfo        prodInfo;
    hycast::SegId           segId;
    hycast::SegInfo         segInfo;
    char                    memData[1000];
    hycast::MemSeg          memSeg;

    PeerTest()
        : state{INIT}
        , srvrAddr{"localhost:38800"}
        /*
         * 3 potential port numbers for the client's 2 temporary servers because
         * the initial client connection could use one
         */
        , portPool(38801, 3)
        , mutex{}
        , cond{}
        , prodId{1}
        , prodSize{1000000}
        , segSize{sizeof(memData)}
        , prodInfo{prodId, prodSize, "product"}
        , segId(prodId, segSize)
        , segInfo(segId, prodSize, segSize)
        , memData{}
        , memSeg{segInfo, memData}
    {
        ::memset(memData, 0xbd, segSize);
    }

public:
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

    // Receiver-side
    bool shouldRequest(
            const hycast::ChunkId   chunkId,
            const hycast::SockAddr& rmtAddr)
    {
        if (chunkId.isProdIndex()) {
            EXPECT_EQ(prodId, chunkId.getProdIndex());
            orState(PROD_NOTICE_RCVD);
        }
        else {
            EXPECT_EQ(segId, chunkId.getSegId());
            orState(SEG_NOTICE_RCVD);
        }

        return true;
    }

    // Receiver-side
    bool shouldRequest(
            const hycast::SegId&    actual,
            const hycast::SockAddr& rmtAddr)
    {
        EXPECT_EQ(segId, actual);
        orState(SEG_NOTICE_RCVD);

        return true;
    }

    // Sender-side
    const hycast::OutChunk& get(
            const hycast::ChunkId   chunkId,
            const hycast::SockAddr& rmtAddr)
    {
        if (chunkId.isProdIndex()) {
            EXPECT_EQ(prodId, chunkId.getProdIndex());
            orState(PROD_REQUEST_RCVD);
            return prodInfo;
        }

        EXPECT_EQ(segId, chunkId.getSegId());
        orState(SEG_REQUEST_RCVD);
        return memSeg;
    }

    // Receiver-side
    bool hereIs(
            const hycast::ProdInfo& actual,
            const hycast::SockAddr& rmtAddr)
    {
        EXPECT_EQ(prodInfo, actual);
        orState(PROD_INFO_RCVD);

        return true;
    }

    // Receiver-side
    bool hereIs(
            hycast::TcpSeg&         actual,
            const hycast::SockAddr& rmtAddr)
    {
        const hycast::SegSize size = actual.getInfo().getSegSize();
        EXPECT_EQ(segSize, size);

        char buf[size];
        actual.read(buf);

        EXPECT_EQ(0, ::memcmp(memSeg.getData(), buf, segSize));

        orState(SEG_RCVD);

        return true;
    }

    void runServer()
    {
        try {
            hycast::TcpSrvrSock srvrSock(srvrAddr);

            orState(LISTENING);

            hycast::TcpSock   peerSock{srvrSock.accept()};
            hycast::PeerProto peerProto(peerSock, portPool);
            hycast::Peer      srvrPeer{peerProto, *this};

            hycast::InetAddr localhost("127.0.0.1");
            EXPECT_EQ(localhost, srvrPeer.getRmtAddr().getInetAddr());

            srvrPeer();
        }
        catch (const std::exception& ex) {
            hycast::log_error(ex);
            throw;
        }
        catch (...) {
            LOG_NOTE("Server thread cancelled");
            throw;
        }
    }
};

// Tests default construction
TEST_F(PeerTest, DefaultConstruction)
{
    hycast::Peer job();
}

// Tests data exchange
TEST_F(PeerTest, DataExchange)
{
    // Start the server
    std::thread srvrThread(&PeerTest::runServer, this);

    try {
        waitForState(LISTENING);

        // Start the client
        hycast::PeerProto peerProto(srvrAddr);
        hycast::Peer      clntPeer{peerProto, *this}; // Potentially slow
        std::thread       clntThread(clntPeer);

        try {
            // Start an exchange
            clntPeer.notify(prodId);
            clntPeer.notify(segId);

            // Wait for the exchange to complete
            waitForState(DONE);

            // Causes `rcvrPeer()` to return and `srvrThread` to terminate
            clntPeer.halt();
            clntThread.join();
            srvrThread.join();
        }
        catch (const std::exception& ex) {
            hycast::log_fatal(ex);
            clntPeer.halt();
            clntThread.join();
            throw;
        }
        catch (...) {
            LOG_FATAL("Thread cancellation?");
            clntThread.join();
            throw;
        } // `rcvrThread` active
    } // `sndrThread` active
    catch (const std::exception& ex) {
        hycast::log_fatal(ex);
        srvrThread.join();
        throw;
    }
    catch (...) {
        LOG_FATAL("Thread cancellation?");
        srvrThread.join();
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
  hycast::log_setLevel(hycast::LOG_LEVEL_TRACE);

  std::set_terminate(&myTerminate);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
