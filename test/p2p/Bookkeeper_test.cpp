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
        std::list<Peer> peers;
        for (;;) {
            peers.push_back(pubPeerSrvr->accept(*this));
        }
    }

protected:
    SockAddr           pubAddr;
    PubPeerSrvr::Pimpl pubPeerSrvr;
    std::thread        pubPeerSrvrThrd;
    ProdIndex          prodIndex;
    DataSegId          segId;
    SubPeer            peer1;
    SubPeer            peer2;

    BookkeeperTest()
        : pubAddr{"localhost:38800"}
        , pubPeerSrvr(PubPeerSrvr::create(pubAddr))
        , pubPeerSrvrThrd(&BookkeeperTest::runPubPeerSrvr, this)
        , prodIndex{1}
        , segId(prodIndex, DataSeg::CANON_DATASEG_SIZE) // Second data-segment
        , peer1(*this, SubRpc::create(pubAddr))
        , peer2(*this, SubRpc::create(pubAddr))
    {}

    ~BookkeeperTest() {
        ::pthread_cancel(pubPeerSrvrThrd.native_handle());
        pubPeerSrvrThrd.join();
    }

public:
    // Both sides
    void waitForSrvrPeer() override {}

    SockAddr getSrvrAddr() const override {
        return SockAddr();
    }

    // Subscriber-side
    bool recvNotice(
            const ProdIndex notice,
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
            const ProdIndex request,
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
            const ProdIndex prodIndex,
            const SockAddr  rmtAddr) {
    }

    void missed(
            const DataSegId dataSegId,
            const SockAddr  rmtAddr) {
    }

    void notify(const ProdIndex prodIndex) {
    }

    void notify(const DataSegId dataSegId) {
    }

    // Subscriber-side
    void recvData(
            const Tracker  tracker,
            const SockAddr rmtAddr) override
    {}

    // Subscriber-side
    void recvData(
            const SockAddr srvrAddr,
            const SockAddr rmtAddr) override
    {}

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

    void lostConnection(Peer peer) override {
        LOG_INFO("Lost connection with peer %s", peer.to_string().data());
    }
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
    SubPeer    peer{*this, pubAddr};

    bookkeeper->add(peer);
}

// Tests making a request
TEST_F(BookkeeperTest, ShouldRequest)
{
    auto bookkeeper = Bookkeeper::createSub();

    bookkeeper->add(peer1);

    EXPECT_TRUE(bookkeeper->shouldRequest(peer1, prodIndex));
    EXPECT_TRUE(bookkeeper->shouldRequest(peer1, segId));

    EXPECT_THROW(bookkeeper->shouldRequest(peer2, prodIndex), LogicError);
    EXPECT_THROW(bookkeeper->shouldRequest(peer2, segId), LogicError);

    ASSERT_TRUE(bookkeeper->add(peer2));
    EXPECT_FALSE(bookkeeper->shouldRequest(peer2, prodIndex));
    EXPECT_FALSE(bookkeeper->shouldRequest(peer2, segId));
    ASSERT_FALSE(bookkeeper->add(peer2));

    ASSERT_TRUE(bookkeeper->received(peer1, prodIndex));
    ASSERT_TRUE(bookkeeper->received(peer1, segId));

    auto worstPeer = bookkeeper->getWorstPeer();
    ASSERT_TRUE(worstPeer);
    EXPECT_NE(peer1, worstPeer);
    EXPECT_EQ(peer2, worstPeer);

    EXPECT_TRUE(bookkeeper->erase(peer1));
    EXPECT_FALSE(bookkeeper->erase(peer1));

    EXPECT_THROW(bookkeeper->shouldRequest(peer1, prodIndex), LogicError);
    EXPECT_THROW(bookkeeper->shouldRequest(peer1, segId), LogicError);
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
    pubPeerSet.notify(prodIndex);
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
