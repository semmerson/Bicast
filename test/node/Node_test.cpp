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
#include "HycastProto.h"
#include "Node.h"
#include "SubInfo.h"
#include "ThreadException.h"

#include <condition_variable>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

using namespace hycast;

/// The fixture for testing class `Node`
class NodeTest : public ::testing::Test
{
protected:
    static constexpr SegSize  SEG_SIZE = 2048;
    static constexpr ProdSize PROD_SIZE = 4095;
    Mutex                     mutex;
    Cond                      cond;
    char                      prodData[PROD_SIZE];
    const String              prodName;
    const ProdId              prodId;
    const String              testRoot;
    const String              pubRepoRoot;
    const String              subRepoRoot;
    const long                maxOpenFiles;
    const String              filePath;
    const ProdInfo            prodInfo;
    const DataSegId           segId;
    const SockAddr            mcastAddr;
    const InetAddr            ifaceAddr;
    const SockAddr            pubP2pAddr;
    const SockAddr            subP2pAddr;
    const int                 listenSize;
    const unsigned            maxPeers;
    int                       numPeers;
    ThreadEx                  threadEx;
    SubInfo                   subInfo;

    NodeTest()
        : mutex()
        , cond()
        , prodData{}
        , prodName{"foo/bar/product.dat"}
        , prodId{prodName}
        , testRoot("/tmp/Node_test")
        , pubRepoRoot(testRoot + "/pub")
        , subRepoRoot(testRoot + "/sub")
        , maxOpenFiles(25)
        , filePath(testRoot + "/" + prodName)
        , prodInfo(prodId, prodName, PROD_SIZE)
        , segId(prodId, 0)
        , mcastAddr("232.1.1.1:3880")
        //, ifaceAddr{"127.0.0.1"}    // Causes PDUs to be received via multicast (see "Sending")
        , ifaceAddr{"192.168.58.133"} // Causes PDUs to be received via P2P (see "Sending")
        , pubP2pAddr{ifaceAddr, 0}
        , subP2pAddr{ifaceAddr, 0}
        , listenSize{0}
        , maxPeers{3}
        , numPeers{0}
        , threadEx()
        , subInfo()
    {
        subInfo.mcast.dstAddr = mcastAddr;
        subInfo.mcast.srcAddr = ifaceAddr;
        subInfo.maxSegSize = SEG_SIZE;
        DataSeg::setMaxSegSize(SEG_SIZE);
        FileUtil::rmDirTree(testRoot);
        int i = 0;
        for (ProdSize offset = 0; offset < PROD_SIZE; offset += SEG_SIZE) {
            auto str = std::to_string(i++);
            int c = str[str.length()-1];
            ::memset(prodData+offset, c, DataSeg::size(PROD_SIZE, offset));
       }
    }

    virtual ~NodeTest() {
        //rmDirTree(testRoot);
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp() {
    }

    virtual void TearDown() {
    }

public:
    void runNode(NodePtr node) {
        try {
            node->run();
        }
        catch (const std::exception& ex) {
            threadEx.set();
        }
    }
};

// Tests construction
TEST_F(NodeTest, Construction)
{
    LOG_NOTE("Creating publishing node");
    auto pubNode = PubNode::create(pubP2pAddr, maxPeers, 60, mcastAddr, ifaceAddr, 1, pubRepoRoot,
            SEG_SIZE, maxOpenFiles, 3600);

    const auto pubP2pSrvrAddr = pubNode->getP2pSrvrAddr();
    const auto pubInetAddr = pubP2pSrvrAddr.getInetAddr();
    EXPECT_EQ(ifaceAddr, pubInetAddr);
    const auto pubPort = pubP2pSrvrAddr.getPort();
    EXPECT_NE(0, pubPort);

    LOG_NOTE("Creating subscribing node");
    subInfo.tracker.insert(pubP2pSrvrAddr);
    auto subNode = SubNode::create(subInfo, ifaceAddr, subP2pAddr, 5, -1, maxPeers, 60, subRepoRoot,
            maxOpenFiles);

    const auto subP2pSrvrAddr = subNode->getP2pSrvrAddr();
    const auto subInetAddr = subP2pSrvrAddr.getInetAddr();
    EXPECT_EQ(ifaceAddr, subInetAddr);
    const auto subPort = subP2pSrvrAddr.getPort();
    EXPECT_NE(0, subPort);
}

#if 1
/**
 * Tests sending.
 *
 * If the loopback interface is used, then the PDUs will be received via multicast. If the external
 * interface is used, however, then the PDUs will be received via the P2P network. The reason for
 * this is that multi-threaded transmission and reception isn't supported for an external interface.
 * The reason for that is unknown.
 */
TEST_F(NodeTest, Sending)
{
    try {
        // Create product-file
        FileUtil::ensureParent(filePath, 0700);
        const int fd = ::open(filePath.data(), O_WRONLY|O_CREAT|O_EXCL, 0600);
        EXPECT_NE(-1, fd);
        for (ProdSize offset = 0; offset < PROD_SIZE; offset += SEG_SIZE) {
            const auto size = DataSeg::size(PROD_SIZE, offset);
            EXPECT_EQ(size, ::write(fd, prodData+offset, size));
        }
        EXPECT_EQ(0, ::close(fd));

        // Create publisher
        auto       pubNode = PubNode::create(pubP2pAddr, maxPeers, 60, mcastAddr, ifaceAddr, 1,
                pubRepoRoot, SEG_SIZE, maxOpenFiles, 3600);
        const auto pubP2pSrvrAddr = pubNode->getP2pSrvrAddr();
        Thread     pubThread(&NodeTest::runNode, this, pubNode);

        // Create subscriber
        subInfo.tracker.insert(pubP2pSrvrAddr);
        auto   subNode = SubNode::create(subInfo, ifaceAddr, subP2pAddr, 5, -1, maxPeers, 60,
                subRepoRoot, maxOpenFiles);
        Thread subThread(&NodeTest::runNode, this, subNode);

        // Wait for subscriber to connect to publisher
        pubNode->waitForPeer();

        // Publish
        const auto repoProdPath = pubRepoRoot + '/' + prodName;
        FileUtil::ensureParent(repoProdPath);
        FileUtil::hardLink(filePath, repoProdPath);

        // Verify reception
        try {
            auto prodInfo = subNode->getNextProd().getProdInfo();

            EXPECT_FALSE(threadEx);
            /*
             * The creation-time can't be compared because it's constructed by the repository;
             * consequently, the times will differ.
             */
            ASSERT_TRUE(NodeTest::prodInfo.getId() == prodInfo.getId());
            ASSERT_TRUE(NodeTest::prodInfo.getName() == prodInfo.getName());
            ASSERT_TRUE(NodeTest::prodInfo.getSize() == prodInfo.getSize());

            for (SegOffset offset = 0; offset < PROD_SIZE; offset += SEG_SIZE) {
                DataSegId  segId(prodId, offset);
                const auto dataSeg = subNode->getDataSeg(segId);
                const auto size = DataSeg::size(PROD_SIZE, offset);
                ASSERT_EQ(size, dataSeg.getSize());
                ASSERT_EQ(0, ::memcmp(prodData+offset, dataSeg.getData(), size));
            }
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex, "Couldn't verify repository access");
            GTEST_FAIL();
        }

        subNode->halt();
        pubNode->halt();

        subThread.join();
        pubThread.join();
    }
    catch (const std::exception& ex) {
        LOG_ERROR("Failure");
        throw;
    }
}
#endif

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
  log_setName(::basename(argv[0]));
  log_setLevel(LogLevel::DEBUG);

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
