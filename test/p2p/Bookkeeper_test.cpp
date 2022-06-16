#include "config.h"

#include "Bookkeeper.h"
#include "logging.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <P2pMgr.h>

#include <list>
#include <mutex>
#include <thread>

namespace {

using namespace hycast;

/// The fixture for testing class `Bookkeeper`
class BookkeeperTest : public ::testing::Test, public SubP2pMgr
{
    void runPubPeerSrvr() {
        std::list<Peer::Pimpl> peers;
        for (;;) {
            peers.push_back(pubPeerSrvr->accept(*this));
        }
    }

protected:
    SockAddr           pubAddr;
    PubP2pSrvr::Pimpl pubPeerSrvr;
    std::thread        pubPeerSrvrThrd;
    ProdId             prodId;
    DataSegId          segId;
    Peer::Pimpl        subPeer1;
    Peer::Pimpl        subPeer2;
    const SegSize      maxSegSize = 1400;

    BookkeeperTest()
        : pubAddr{"localhost:38800"}
        , pubPeerSrvr(PubP2pSrvr::create(pubAddr, 5))
        , pubPeerSrvrThrd(&BookkeeperTest::runPubPeerSrvr, this)
        , prodId{"product"}
        , segId(prodId, maxSegSize) // Second data-segment
        , subPeer1(Peer::create(*this, pubAddr))
        , subPeer2(Peer::create(*this, pubAddr))
    {
        DataSeg::setMaxSegSize(maxSegSize);
    }

    ~BookkeeperTest() {
        ::pthread_cancel(pubPeerSrvrThrd.native_handle());
        pubPeerSrvrThrd.join();
    }

public:
    void start() {};
    void stop() {};
    void run() {};
    void halt() {};

    // Both sides
    void waitForSrvrPeer() override {}

    SockAddr getSrvrAddr() const override {
        return SockAddr();
    }

    // Subscriber-side
    bool recvNotice(
            const ProdId notice,
            const SockAddr  rmtAddr) override
    {
        return false;
    }

    // Subscriber-side
    bool recvNotice(
            const DataSegId notice,
            const SockAddr  rmtAddr) override
    {
        return false;
    }

    // Publisher-side
    ProdInfo recvRequest(
            const ProdId request,
            const SockAddr  rmtAddr) override
    {
        return ProdInfo{};
    }

    // Publisher-side
    DataSeg recvRequest(
            const DataSegId request,
            const SockAddr  rmtAddr) override
    {
        return DataSeg{};
    }

    void missed(
            const ProdId   prodId,
            const SockAddr rmtAddr) {
    }

    void missed(
            const DataSegId dataSegId,
            const SockAddr  rmtAddr) {
    }

    void notify(const ProdId prodId) {
    }

    void notify(const DataSegId dataSegId) {
    }

    // Subscriber-side
    void recvData(
            const ProdInfo data,
            const SockAddr rmtAddr) override
    {}

    // Subscriber-side
    void recvData(
            const DataSeg  actualDataSeg,
            const SockAddr rmtAddr) override
    {}
};

// Tests default construction
TEST_F(BookkeeperTest, DefaultConstruction)
{
    Bookkeeper::createPub();
    Bookkeeper::createSub();
}

// Tests adding a peerSubP2pNode
TEST_F(BookkeeperTest, PeerAddition)
{
    auto bookkeeper = Bookkeeper::createSub();
    auto subPeer = Peer::create(*this, pubAddr);

    bookkeeper->add(subPeer);
}

// Tests making a request
TEST_F(BookkeeperTest, ShouldRequest)
{
    auto bookkeeper = Bookkeeper::createSub();

    bookkeeper->add(subPeer1);

    EXPECT_TRUE(bookkeeper->shouldRequest(subPeer1, prodId));
    EXPECT_TRUE(bookkeeper->shouldRequest(subPeer1, segId));

    EXPECT_THROW(bookkeeper->shouldRequest(subPeer2, prodId), LogicError);
    EXPECT_THROW(bookkeeper->shouldRequest(subPeer2, segId), LogicError);

    ASSERT_TRUE(bookkeeper->add(subPeer2));
    EXPECT_FALSE(bookkeeper->shouldRequest(subPeer2, prodId));
    EXPECT_FALSE(bookkeeper->shouldRequest(subPeer2, segId));
    ASSERT_FALSE(bookkeeper->add(subPeer2));

    ASSERT_TRUE(bookkeeper->received(subPeer1, prodId));
    ASSERT_TRUE(bookkeeper->received(subPeer1, segId));

    auto worstPeer = bookkeeper->getWorstPeer();
    EXPECT_NE(subPeer1, worstPeer);
    EXPECT_EQ(subPeer2, worstPeer);

    EXPECT_TRUE(bookkeeper->erase(subPeer1));
    EXPECT_FALSE(bookkeeper->erase(subPeer1));

    EXPECT_THROW(bookkeeper->shouldRequest(subPeer1, prodId), LogicError);
    EXPECT_THROW(bookkeeper->shouldRequest(subPeer1, segId), LogicError);
}

#if 0
// Tests data exchange
TEST_F(BookkeeperTest, DataExchange)
{
    // Create and execute reception by publishing peer on separate thread
    std::thread srvrThread(&BookkeeperTest::startPubPeer, this);

    waitForState(LISTENING);

    // Create and execute reception by subscribing peers on separate threads
    PeerSet subPeerSet{};
    for (int i = 0; i < NUM_PEERS; ++i) {
        Peer subPeer = Peer(pubAddr, *this);
        ASSERT_TRUE(subPeerSet.insert(subPeer)); // Starts reading
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
#endif

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
