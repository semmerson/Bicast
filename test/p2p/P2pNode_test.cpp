/**
 * This file tests class `P2pNode`.
 *
 * Copyright 2021 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: P2pNode_test.cpp
 * Created On: Oct 3, 2021
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "logging.h"
#include "P2pNode.h"

#include <cstring>
#include <gtest/gtest.h>
#include <iostream>

namespace {

using namespace hycast;

/// The fixture for testing class `P2pNode`
class P2pNodeTest : public ::testing::Test, public SubNode
{
protected:
    static constexpr SegSize SEG_SIZE = DataSeg::CANON_DATASEG_SIZE;

    SockAddr  pubPeerSrvrAddr;
    SockAddr  subPeerSrvrAddr;
    ProdIndex prodIndex;
    char      data[100000];
    ProdInfo  prodInfo;

    P2pNodeTest()
        : pubPeerSrvrAddr("127.0.0.1:38800")
        , subPeerSrvrAddr("127.0.0.1:0")
        , prodIndex(1)
        , data()
        , prodInfo(prodIndex, "product", sizeof(data))
    {
        ::memset(data, 0xbd, sizeof(data));
    }

    virtual ~P2pNodeTest()
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

    void notify(std::shared_ptr<P2pNode> p2pNode) {
        p2pNode->notify(prodIndex);
        for (ProdSize offset = 0; offset < sizeof(data); offset += SEG_SIZE) {
            DataSegId dataSegId(prodIndex, offset);
            p2pNode->notify(dataSegId);
        }
    }

public:
    ProdInfo recvRequest(const ProdIndex request) {
        // TODO
    }

    DataSeg recvRequest(const DataSegId request) {
        // TODO
    }
};

// Tests construction of publisher's P2P node
TEST_F(P2pNodeTest, PubP2pNodeCtor)
{
    auto pubP2pNode = hycast::P2pNode::create(*this, pubPeerSrvrAddr, 8,
            SEG_SIZE);
    const auto pathToPub = pubP2pNode->isPathToPub();
    EXPECT_TRUE(pathToPub);
}

// Tests a single subscriber
TEST_F(P2pNodeTest, SingleSubscriber)
{
    auto pubP2pNode = P2pNode::create(*this, pubPeerSrvrAddr, 8, SEG_SIZE);
    PeerSrvrAddrs peerSrvrAddrs{};
    peerSrvrAddrs.push(pubPeerSrvrAddr);
    auto subP2pNode = SubP2pNode::create(*this, pubPeerSrvrAddr,
            peerSrvrAddrs, subPeerSrvrAddr, 8, SEG_SIZE);

    notify(pubP2pNode);
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
  hycast::log_setLevel(hycast::LogLevel::TRACE);

  std::set_terminate(&myTerminate);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
