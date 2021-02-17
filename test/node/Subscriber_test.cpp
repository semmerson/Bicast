/**
 * This file tests class `Receiver`.
 *
 *       File: Receiver_test.cpp
 * Created On: Jan 3, 2020
 *     Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"
#include "FileUtil.h"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <main/node/Subscriber.h>
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
    hycast::SubRepo       repo;
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
    hycast::Subscriber receiver(sndrSockAddr, listenSize, portPool, maxPeers,
            grpSockAddr, sndrInetAddr, repo);
}

}  // namespace

int main(int argc, char **argv) {
  hycast::log_setName(::basename(argv[0]));
  hycast::log_setLevel(hycast::LOG_LEVEL_DEBUG);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
