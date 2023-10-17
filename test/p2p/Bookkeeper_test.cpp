#include "config.h"

#include "Bookkeeper.h"
#include "logging.h"
#include "P2pMgr.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <list>
#include <mutex>
#include <thread>

namespace {

using namespace bicast;

/// The fixture for testing class `Bookkeeper`
class BookkeeperTest : public ::testing::Test
{
protected:
    ProdId        prodId;
    DataSegId     segId;
    SockAddr      rmtAddr1;
    SockAddr      rmtAddr2;
    const SegSize maxSegSize = 1400;

    BookkeeperTest()
        : prodId{"product"}
        , segId(prodId, maxSegSize) // Second data-segment
        , rmtAddr1("localhost:38801")
        , rmtAddr2("localhost:38802")
    {
        DataSeg::setMaxSegSize(maxSegSize);
    }

    ~BookkeeperTest() {
    }

public:
};

// Tests default construction
TEST_F(BookkeeperTest, DefaultConstruction)
{
    Bookkeeper::createPub();
    Bookkeeper::createSub();
}

// Tests adding a subscribing peer
TEST_F(BookkeeperTest, PeerAddition)
{
    auto bookkeeper = Bookkeeper::createSub();
    bookkeeper->add(rmtAddr1, true);
}

// Tests making a request
TEST_F(BookkeeperTest, ShouldRequest)
{
    auto bookkeeper = Bookkeeper::createSub();

    bookkeeper->add(rmtAddr1, true);

    EXPECT_TRUE(bookkeeper->shouldRequest(rmtAddr1, prodId));
    EXPECT_TRUE(bookkeeper->shouldRequest(rmtAddr1, segId));

    EXPECT_THROW(bookkeeper->shouldRequest(rmtAddr2, prodId), LogicError);
    EXPECT_THROW(bookkeeper->shouldRequest(rmtAddr2, segId), LogicError);

    ASSERT_TRUE(bookkeeper->add(rmtAddr2, true));
    EXPECT_FALSE(bookkeeper->shouldRequest(rmtAddr2, prodId));
    EXPECT_FALSE(bookkeeper->shouldRequest(rmtAddr2, segId));
    ASSERT_FALSE(bookkeeper->add(rmtAddr2, true));

    ASSERT_TRUE(bookkeeper->received(rmtAddr1, prodId));
    ASSERT_TRUE(bookkeeper->received(rmtAddr1, segId));

    auto worstPeerAddr = bookkeeper->getWorstPeer();
    EXPECT_NE(rmtAddr1, worstPeerAddr);
    EXPECT_EQ(rmtAddr2, worstPeerAddr);

    EXPECT_TRUE(bookkeeper->erase(rmtAddr1));
    EXPECT_FALSE(bookkeeper->erase(rmtAddr1));

    EXPECT_THROW(bookkeeper->shouldRequest(rmtAddr1, prodId), LogicError);
    EXPECT_THROW(bookkeeper->shouldRequest(rmtAddr1, segId), LogicError);
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
