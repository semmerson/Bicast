#include "config.h"

#include "error.h"
#include "SockAddr.h"
#include "Wire.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <Peer.h>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `Peer`
class PeerTest : public ::testing::Test, public hycast::PeerMsgRcvr
{
protected:
    hycast::SockAddr        srvrAddr;
    hycast::SockAddr        clntAddr;
    hycast::Peer            srcPeer;
    hycast::Peer            snkPeer;
    std::mutex              mutex;
    std::condition_variable cond;
    typedef enum {
        INIT,
        LISTENING,
        CONNECTED,
        DONE
    }                       State;
    State                   state;
    hycast::ChunkId         chunkId;
    char                    memData[1000] = {0};
    hycast::MemChunk        memChunk;
    hycast::Peer            srvrPeer;
    hycast::Peer            clntPeer;
    // You can remove any or all of the following functions if its body
    // is empty.

    PeerTest()
        : srvrAddr{"localhost:38800"}
        , clntAddr{}
        , mutex{}
        , cond{}
        , state{INIT}
        , chunkId{1}
        , memChunk(chunkId, sizeof(memData), memData)
        , srvrPeer()
        , clntPeer()
    {
        // You can do set-up work for each test here.
    }

    virtual ~PeerTest()
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

public:
    void setState(const State newState)
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
        state = newState;
        cond.notify_one();
    }

    void waitForState(const State nextState)
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (state != nextState)
            cond.wait(lock);
    }

    // Sink-side
    bool shouldRequest(
            const hycast::ChunkId&  notice,
            const hycast::SockAddr& rmtAddr)
    {
        EXPECT_EQ(chunkId, notice);
        EXPECT_EQ(0, snkPeer.size());
        EXPECT_TRUE(snkPeer.begin() == snkPeer.end());

        return true;
    }

    // Source-side
    hycast::MemChunk get(
            const hycast::ChunkId&  request,
            const hycast::SockAddr& rmtAddr)
    {
        EXPECT_EQ(1, snkPeer.size());
        EXPECT_TRUE(snkPeer.begin() != snkPeer.end());
        EXPECT_EQ(0, srcPeer.size());
        EXPECT_TRUE(srcPeer.begin() == srcPeer.end());
        EXPECT_EQ(chunkId, request);

        return memChunk;
    }

    // Sink-side
    void hereIs(
            hycast::WireChunk       wireChunk,
            const hycast::SockAddr& rmtAddr)
    {
        EXPECT_EQ(1, snkPeer.size());
        EXPECT_TRUE(snkPeer.begin() != snkPeer.end());

        EXPECT_EQ(0, srcPeer.size());
        EXPECT_TRUE(srcPeer.begin() == srcPeer.end());

        const hycast::ChunkSize n = wireChunk.getSize();
        char                    wireData[n];

        wireChunk.read(wireData);
        for (int i = 0; i < n; ++i)
            EXPECT_EQ(memData[i], wireData[i]);

        setState(DONE);
    }

    // Sink-side
    bool shouldRequest(
            const hycast::ChunkId& notice,
            hycast::Peer           snkPeer)
    {
        EXPECT_TRUE((snkPeer == srvrPeer) || (snkPeer == clntPeer));
        EXPECT_EQ(chunkId, notice);
        EXPECT_EQ(0, snkPeer.size());
        EXPECT_TRUE(snkPeer.begin() == snkPeer.end());

        return true;
    }

    // Source-side
    hycast::MemChunk get(
            const hycast::ChunkId& request,
            hycast::Peer           srcPeer)
    {
        EXPECT_EQ(1, snkPeer.size());
        EXPECT_TRUE(snkPeer.begin() != snkPeer.end());
        EXPECT_EQ(0, srcPeer.size());
        EXPECT_TRUE(srcPeer.begin() == srcPeer.end());
        EXPECT_EQ(chunkId, request);

        return memChunk;
    }

    // Sink-side
    void hereIs(
            hycast::WireChunk wireChunk,
            hycast::Peer      snkPeer)
    {
        EXPECT_EQ(1, snkPeer.size());
        EXPECT_TRUE(snkPeer.begin() != snkPeer.end());

        EXPECT_EQ(0, srcPeer.size());
        EXPECT_TRUE(srcPeer.begin() == srcPeer.end());

        const hycast::ChunkSize n = wireChunk.getSize();
        char                    wireData[n];

        wireChunk.read(wireData);
        for (int i = 0; i < n; ++i)
            EXPECT_EQ(memData[i], wireData[i]);

        setState(DONE);
    }

    void runServer()
    {
        try {
            hycast::SrvrSock srvrSock(srvrAddr);

            setState(LISTENING);

            hycast::Socket     peerSock{srvrSock.accept()};

            srvrPeer = hycast::Peer(peerSock, *this);
            hycast::InAddr localhost("127.0.0.1");
            EXPECT_EQ(localhost, srvrPeer.getRmtAddr().getInAddr());
            setState(CONNECTED);

            srvrPeer();
        }
        catch (const std::runtime_error& ex) {
            LOG_NOTE("Remote peer closed the connection");
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex, "Couldn't run peer-server");
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
    // Start the peer-server
    std::thread srvrThread(&PeerTest::runServer, this);

    waitForState(LISTENING);

    // Start the client-peer
    hycast::PortPool   portPool{38801, 2};
    clntPeer = hycast::Peer(srvrAddr, portPool, *this); // Potentially slow
    EXPECT_EQ(srvrAddr, clntPeer.getRmtAddr());
    clntAddr = clntPeer.getLclAddr();
    std::thread clntThread(clntPeer);

    waitForState(CONNECTED);

    // Establish the source and sink peers
    srcPeer = srvrPeer;
    snkPeer = clntPeer;

    // Start an exchange
    const bool enqueued = srcPeer.notify(chunkId);
    EXPECT_TRUE(enqueued);

    // Wait for the exchange to complete
    waitForState(DONE);

    /*
     * The sink-peer calls `hereIs()` before removing it from the pending
     * set; therefore, the sink-peer isn't tested.
     */
    EXPECT_EQ(0, srcPeer.size());
    EXPECT_TRUE(srcPeer.begin() == srcPeer.end());

    // Causes `clntPeer()` to return and `srvrThread` to terminate
    clntPeer.halt();
    clntThread.join();
    srvrThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  hycast::log_setName(::basename(argv[0]));
  hycast::log_setLevel(hycast::LOG_LEVEL_TRACE);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
