/**
 * This file tests class `Node`
 *
 *       File: Node_test.cpp
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

#include "error.h"
#include "FileUtil.h"
#include "hycast.h"
#include "Node.h"

#include <condition_variable>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

/// The fixture for testing class `Node`
class NodeTest : public ::testing::Test
{
protected:
    std::mutex                mutex;
    std::condition_variable   cond;
    char                      memData[1000];
    const hycast::SegSize     segSize;
    const std::string         prodName;
    const std::string         testRoot;
    const std::string         pubRepoRoot;
    const std::string         subRepoRoot;
    hycast::PubRepo           pubRepo;
    hycast::SubRepo           subRepo;
    const std::string         filePath;
    const hycast::ProdIndex   prodIndex;
    const hycast::ProdSize    prodSize;
    const hycast::ProdInfo    prodInfo;
    const hycast::SegId       segId;
    const hycast::SegInfo     segInfo;
    hycast::MemSeg            memSeg;
    const hycast::SockAddr    grpSockAddr;
    const hycast::InetAddr    pubInetAddr;
    const hycast::InetAddr    subInetAddr;
    const hycast::SockAddr    pubSockAddr;
    const hycast::SockAddr    subSockAddr;
    const int                 listenSize;
    const int                 maxPeers;
    hycast::P2pInfo           pubP2pInfo;
    hycast::P2pInfo           subP2pInfo;
    hycast::SrcMcastAddrs     srcMcastInfo;
    hycast::ServerPool        p2pSrvrPool;
    int                       numPeers;

    NodeTest()
        : mutex()
        , cond()
        , memData{}
        , segSize{sizeof(memData)}
        , prodIndex{1}
        , prodSize{5000}
        , prodName{"foo/bar/product.dat"}
        , testRoot("/tmp/Node_test")
        , pubRepoRoot(testRoot + "/pub")
        , subRepoRoot(testRoot + "/sub")
        , pubRepo() // Can't initialize because `rmDirTree(testRoot)`
        , subRepo() // Can't initialize because `rmDirTree(testRoot)`
        , filePath(testRoot + "/" + prodName)
        , prodInfo{prodIndex, prodSize, prodName}
        , segId(prodIndex, 0)
        , segInfo(segId, prodSize, segSize)
        , memSeg{segInfo, memData}
        , grpSockAddr("232.1.1.1:3880")
        , pubInetAddr{"192.168.174.141"}
        , pubSockAddr{pubInetAddr, 3880}
        , subInetAddr{"192.168.174.141"}
        , subSockAddr{subInetAddr, 3881} // NB: Not a Linux dynamic port number
        , listenSize{0}
        , maxPeers{3}
        , pubP2pInfo{.sockAddr=pubSockAddr, .listenSize=listenSize,
            .maxPeers=maxPeers}
        , subP2pInfo{.sockAddr=subSockAddr, .listenSize=listenSize,
            .maxPeers=maxPeers}
        , srcMcastInfo{.grpAddr=grpSockAddr, .srcAddr=pubInetAddr}
        , p2pSrvrPool{std::set<hycast::SockAddr>{pubSockAddr}}
        , numPeers{0}
    {
        hycast::rmDirTree(testRoot);
        ::memset(memData, 0xbd, segSize);
        pubRepo = hycast::PubRepo(pubRepoRoot, segSize);
        subRepo = hycast::SubRepo(subRepoRoot, segSize);
    }

    virtual ~NodeTest() {
        hycast::rmDirTree(testRoot);
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp() {
    }

    virtual void TearDown() {
    }

public:
    static void runNode(hycast::Node& node) {
        try {
            node();
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Exception thrown: %s", ex.what());
            LOG_ERROR(ex);
        }
        catch (...) {
            LOG_DEBUG("Thread cancelled");
            throw;
        }
    }
};

// Tests publisher construction
TEST_F(NodeTest, PublisherConstruction)
{
    hycast::Publisher(pubP2pInfo, grpSockAddr, pubRepo);
}

// Tests subscriber construction
TEST_F(NodeTest, SubscriberConstruction)
{
    hycast::Subscriber(srcMcastInfo, subP2pInfo, p2pSrvrPool, subRepo);
}

// Tests sending
TEST_F(NodeTest, Sending)
{
    try {
        // Create product-file
        hycast::ensureParent(filePath, 0700);
        int fd = ::open(filePath.data(), O_WRONLY|O_CREAT|O_EXCL, 0600);
        EXPECT_NE(-1, fd);
        for (hycast::ProdSize offset = 0; offset < prodSize; offset += segSize) {
            const hycast::SegSize size = (offset + segSize > prodSize)
                    ? prodSize - offset
                    : segSize;
            EXPECT_EQ(size, ::write(fd, memData, size));
        }
        EXPECT_EQ(0, ::close(fd));

        // Create publisher
        hycast::Publisher publisher(pubP2pInfo, grpSockAddr, pubRepo);
        std::thread       pubThread(&NodeTest::runNode, std::ref(publisher));

        // Create subscriber
        hycast::Subscriber subscriber(srcMcastInfo, subP2pInfo, p2pSrvrPool,
                subRepo);
        std::thread        subThread(&NodeTest::runNode, std::ref(subscriber));

        // Give connection a chance
        sleep(3);

        // Publish
        pubRepo.link(filePath, prodName);

        // Verify reception
        try {
            auto prodInfo = subRepo.getNextProd();
            ASSERT_TRUE(NodeTest::prodInfo == prodInfo);
            for (hycast::ProdSize offset = 0; offset < prodSize;
                    offset += segSize) {
                hycast::SegId segId{prodIndex, offset};
                auto memSeg = subRepo.getMemSeg(segId);
                const auto size = (offset + segSize > prodSize)
                        ? prodSize - offset
                        : segSize;
                ASSERT_EQ(size, memSeg.getSegSize());
                ASSERT_EQ(0, ::memcmp(NodeTest::memSeg.data(), memSeg.data(),
                        size));
            }
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex, "Couldn't verify repository access");
            GTEST_FAIL();
        }

        subscriber.halt();
        publisher.halt(); // Closes connection with subscriber's peer
        subThread.join();
        pubThread.join();
    }
    catch (const std::exception& ex) {
        LOG_ERROR("Failure");
        throw;
    }
}

}  // namespace

static void myTerminate()
{
    auto exPtr = std::current_exception();

    if (!exPtr) {
        LOG_FATAL("terminate() called without an active exception");
    }
    else {
        try {
            std::rethrow_exception(exPtr);
        }
        catch (const std::exception& ex) {
            LOG_FATAL(ex);
        }
        catch (...) {
            LOG_FATAL("terminate() called with a non-standard exception");
        }
    }

    abort();
}

int main(int argc, char **argv) {
  hycast::log_setName(::basename(argv[0]));
  hycast::log_setLevel(hycast::LogLevel::DEBUG);

  /*
   * Ignore SIGPIPE so that writing to a closed socket doesn't terminate the
   * process (the return-value from write() is always checked).
   */
  struct sigaction sigact;
  sigact.sa_handler = SIG_IGN;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = 0;
  (void)sigaction(SIGPIPE, &sigact, NULL);

  std::set_terminate(&myTerminate);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
