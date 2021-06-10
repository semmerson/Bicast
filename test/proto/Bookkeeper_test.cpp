#include "config.h"

#include "Bookkeeper.h"
#include "P2pNode.h"
#include "logging.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `Bookkeeper`
class BookkeeperTest : public ::testing::Test, public hycast::P2pNode
{
protected:
    hycast::SockAddr        pubAddr;
    hycast::ProdIndex       prodIndex;
    hycast::DataSegId       segId;
    hycast::Peer            peer1;
    hycast::Peer            peer2;

    BookkeeperTest()
        : pubAddr{"localhost:38800"}
        , prodIndex{1}
        , segId(prodIndex, hycast::DataSeg::CANON_DATASEG_SIZE) // Second data-segment
        , peer1(pubAddr, *this)
        , peer2(pubAddr, *this)
    {}

public:
    // Publisher-side
    bool isPublisher() const override {}

    // Publisher-side
    bool isPathToPub() const override {}

    // Both sides
    void recvNotice(const hycast::PubPath notice, hycast::Peer peer)
            override
    {}

    // Subscriber-side
    void recvNotice(const hycast::ProdIndex notice, hycast::Peer peer)
            override
    {}

    // Subscriber-side
    void recvNotice(const hycast::DataSegId& notice, hycast::Peer peer)
            override
    {}

    // Publisher-side
    void recvRequest(const hycast::ProdIndex request, hycast::Peer peer)
            override
    {}

    // Publisher-side
    void recvRequest(const hycast::DataSegId& request, hycast::Peer peer)
            override
    {}

    // Subscriber-side
    void recvData(const hycast::ProdInfo& data, hycast::Peer peer) override
    {}

    // Subscriber-side
    void recvData(const hycast::DataSeg& actualDataSeg, hycast::Peer peer)
            override
    {}

    void died(hycast::Peer peer) {
        LOG_ERROR("Peer %s died", peer.to_string().data());
    }
    void reassigned(const hycast::ProdIndex notice,
                    hycast::Peer            peer) {
        EXPECT_EQ(peer, peer2);
    }
    void reassigned(const hycast::DataSegId& notice,
                    hycast::Peer             peer) {
        EXPECT_EQ(peer, peer2);
    }
};

// Tests default construction
TEST_F(BookkeeperTest, DefaultConstruction)
{
    hycast::PubBookkeeper pubBookkeeper{};
    hycast::SubBookkeeper subBookkeeper{};
}

// Tests adding a peer
TEST_F(BookkeeperTest, PeerAddition)
{
    hycast::SubBookkeeper bookkeeper{};
    hycast::Peer          peer{pubAddr, *this};

    bookkeeper.add(peer);
}

// Tests making a request
TEST_F(BookkeeperTest, ShouldRequest)
{
    hycast::SubBookkeeper bookkeeper{};

    bookkeeper.add(peer1);
    bookkeeper.add(peer2);

    ASSERT_FALSE(bookkeeper.received(peer1, prodIndex));

    ASSERT_TRUE(bookkeeper.shouldRequest(peer1, prodIndex));
    ASSERT_TRUE(bookkeeper.shouldRequest(peer1, segId));
    ASSERT_FALSE(bookkeeper.shouldRequest(peer2, prodIndex));
    ASSERT_FALSE(bookkeeper.shouldRequest(peer2, segId));
    ASSERT_THROW(bookkeeper.shouldRequest(peer1, prodIndex), std::logic_error);

    ASSERT_TRUE(bookkeeper.received(peer1, prodIndex));
    ASSERT_FALSE(bookkeeper.received(peer2, prodIndex));

    ASSERT_TRUE(bookkeeper.shouldRequest(peer1, prodIndex));
    bookkeeper.erase(peer1);
    ASSERT_TRUE(bookkeeper.received(peer2, prodIndex));
    ASSERT_THROW(bookkeeper.received(peer1, prodIndex), std::logic_error);

    ASSERT_THROW(bookkeeper.erase(peer1), std::logic_error);
}

#if 0
// Tests data exchange
TEST_F(BookkeeperTest, DataExchange)
{
    // Create and execute reception by publishing peer on separate thread
    std::thread srvrThread(&BookkeeperTest::startPubPeer, this);

    waitForState(LISTENING);

    // Create and execute reception by subscribing peers on separate threads
    hycast::PeerSet subPeerSet{};
    for (int i = 0; i < NUM_PEERS; ++i) {
        hycast::Peer subPeer = hycast::Peer(pubAddr, *this);
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
  hycast::log_setName(::basename(argv[0]));
  //hycast::log_setLevel(hycast::LogLevel::TRACE);

  std::set_terminate(&myTerminate);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
