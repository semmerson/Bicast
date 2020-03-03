/**
 * This file tests class `Receiver`.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: Receiver_test.cpp
 * Created On: Jan 3, 2020
 *     Author: Steven R. Emmerson
 */
#include "config.h"
#include "Receiver.h"

#include "FileUtil.h"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

namespace {

/// The fixture for testing class `Receiver`
class ReceiverTest : public ::testing::Test
{
protected:
    hycast::SockAddr      grpSockAddr;
    hycast::SockAddr      sndrInetAddr;
    hycast::SockAddr      sndrSockAddr;
    hycast::SockAddr      rcvrSockAddr;
    int                   listenSize;
    hycast::PortPool      portPool;
    int                   maxPeers;
    hycast::RcvRepo       repo;
    hycast::ProdIndex     prodIndex;
    char                  memData[1000];
    const hycast::SegSize segSize;
    hycast::ProdSize      prodSize;
    hycast::ProdInfo      prodInfo;
    hycast::SegId         segId;
    hycast::SegInfo       segInfo;
    hycast::MemSeg        memSeg;

    ReceiverTest()
        : grpSockAddr("232.1.1.1:3880")
        , sndrInetAddr{"localhost"}
        , sndrSockAddr{sndrInetAddr, 3880}
        , rcvrSockAddr{"localhost:3881"} // NB: Not a Linux dynamic port number
        , listenSize{0}
        , portPool(38800, 5) // NB: Linux Dynamic port numbers
        , maxPeers{3}
        , repo("repo", 1000)
        , prodIndex{1}
        , memData{}
        , segSize{sizeof(memData)}
        , prodSize{segSize}
        , prodInfo{prodIndex, prodSize, "foo/bar/product.dat"}
        , segId(prodIndex, 0)
        , segInfo(segId, prodSize, segSize)
        , memSeg{segInfo, memData}
    {
        ::memset(memData, 0xbd, segSize);
    }

    virtual ~ReceiverTest()
    {
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
    }

    // Objects declared here can be used by all tests in the test case for Error.
};

// Tests construction
TEST_F(ReceiverTest, Construction)
{
    hycast::Receiver receiver(sndrSockAddr, listenSize, portPool, maxPeers,
            grpSockAddr, sndrInetAddr, repo);
}

}  // namespace

int main(int argc, char **argv) {
  hycast::log_setName(::basename(argv[0]));
  hycast::log_setLevel(hycast::LOG_LEVEL_DEBUG);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
