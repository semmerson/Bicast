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

#include "Disposer.h"
#include "error.h"
#include "FileUtil.h"
#include "HycastProto.h"
#include "Node.h"
#include "SubInfo.h"
#include "ThreadException.h"

#include <chrono>
#include <condition_variable>
#include <fcntl.h>
#include <functional>
#include <gtest/gtest.h>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

using namespace std;
using namespace hycast;

/// The fixture for testing class `Node`
class NodeTest : public ::testing::Test, public SubNode::Client
{
protected:
    static constexpr SegSize  SEG_SIZE = 10000;
    static constexpr ProdSize PROD_SIZE = 100000;
    Mutex                     mutex;
    Cond                      cond;
    ProdInfo                  prodInfo;
    char                      prodData[PROD_SIZE];
    bool                      prodRcvd;
    const String              testRoot;
    const String              pubRoot;
    const String              subRoot;
    const long                maxOpenFiles;
    const SockAddr            mcastAddr;
    const InetAddr            ifaceAddr;
    const SockAddr            pubP2pAddr;
    const SockAddr            subP2pAddr;
    const int                 listenSize;
    const unsigned            maxPeers;
    int                       numPeers;
    ThreadEx                  threadEx;
    SubInfo                   subInfo;
    String                    feedName;
    SubNodePtr                subNodePtr;

    NodeTest()
        : mutex()
        , cond()
        , prodInfo()
        , prodData{}
        , prodRcvd(false)
        , testRoot("/tmp/Node_test")
        , pubRoot(testRoot + "/pub")
        , subRoot(testRoot + "/sub")
        , maxOpenFiles(25)
        , mcastAddr("232.1.1.1:3880")
        //, ifaceAddr{"192.168.58.139"} // Causes PDU reception via P2P only
        , ifaceAddr{"127.0.0.1"}        // Causes PDU reception via P2P & multicast
        , pubP2pAddr{ifaceAddr, 0}
        , subP2pAddr{ifaceAddr, 0}
        , listenSize{0}
        , maxPeers{3}
        , numPeers{0}
        , threadEx()
        , subInfo()
        , feedName("Node_test_feed")
        , subNodePtr()
    {
        subInfo.mcast.dstAddr = mcastAddr;
        subInfo.mcast.srcAddr = ifaceAddr;
        subInfo.maxSegSize = SEG_SIZE;
        DataSeg::setMaxSegSize(SEG_SIZE);
        FileUtil::rmDirTree(testRoot);
        int i = 0;
        for (ProdSize offset = 0; offset < PROD_SIZE; offset += SEG_SIZE) {
            auto str = std::to_string(i++);
            int c = str.back();
            ::memset(prodData+offset, c, DataSeg::size(PROD_SIZE, offset));
       }
    }

    virtual ~NodeTest() {
        FileUtil::rmDirTree(testRoot);
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
            LOG_ERROR(ex);
            threadEx.set();
        }
    }

    void received(const ProdInfo& prodInfo) {
        Guard      guard{mutex};
        EXPECT_TRUE(prodInfo == this->prodInfo);

        const auto prodId = prodInfo.getId();
        for (SegOffset offset = 0; offset < PROD_SIZE; offset += SEG_SIZE) {
            DataSegId  segId(prodId, offset);
            const auto dataSeg = subNodePtr->getDataSeg(segId);
            const auto size = DataSeg::size(PROD_SIZE, offset);
            ASSERT_EQ(size, dataSeg.getSize());
            ASSERT_EQ(0, ::memcmp(prodData+offset, dataSeg.getData(), size));
        }

        prodRcvd = true;
        cond.notify_all();
    }
};

// Tests construction
TEST_F(NodeTest, Construction)
{
    LOG_NOTE("Creating publishing node");
    Tracker tracker{};
    auto pubNode = PubNode::create(tracker, pubP2pAddr, maxPeers, 60, mcastAddr, ifaceAddr, 1,
            pubRoot, SEG_SIZE, maxOpenFiles, 3600, feedName);

    const auto pubP2pSrvrAddr = pubNode->getP2pSrvrAddr();
    const auto pubInetAddr = pubP2pSrvrAddr.getInetAddr();
    EXPECT_EQ(ifaceAddr, pubInetAddr);
    const auto pubPort = pubP2pSrvrAddr.getPort();
    EXPECT_NE(0, pubPort);

    LOG_NOTE("Creating subscribing node");
    subInfo.tracker.insert(pubP2pSrvrAddr);
    auto peerConnSrvr = PeerConnSrvr::create(subP2pAddr, 5);
    Disposer::Factory factory = [&] (const String& pathTemplate) {
        return Disposer{};
    };
    subNodePtr = SubNode::create(subInfo, ifaceAddr, peerConnSrvr, -1, maxPeers, 60, subRoot,
            maxOpenFiles, factory, this); // NB: Invalid dispose-file

    const auto subP2pSrvrAddr = subNodePtr->getP2pSrvrAddr();
    const auto subInetAddr = subP2pSrvrAddr.getInetAddr();
    EXPECT_EQ(ifaceAddr, subInetAddr);
    const auto subPort = subP2pSrvrAddr.getPort();
    EXPECT_NE(0, subPort);
}

#if 1
/**
 * Tests sending and performance.
 *
 * If the loopback interface is used, then the PDUs will be received via multicast and P2P. If the
 * external interface is used, however, then the PDUs will be received solely via the P2P network.
 * The reason for this is that multi-threaded transmission and reception isn't supported for an
 * external interface. The reason for that is unknown.
 */
TEST_F(NodeTest, Sending)
{
    constexpr int NUM_PRODS = 1000;
    const String  prodBaseName("foo/bar/product.dat");
    const String  filePath(FileUtil::pathname(testRoot, prodBaseName));
    steady_clock::time_point start;
    steady_clock::time_point stop;

    try {
        // Create the single product-file
        FileUtil::ensureParent(filePath, 0700);
        const int fd = ::open(filePath.data(), O_WRONLY|O_CREAT|O_EXCL, 0600);
        EXPECT_NE(-1, fd);
        for (ProdSize offset = 0; offset < PROD_SIZE; offset += SEG_SIZE) {
            const auto size = DataSeg::size(PROD_SIZE, offset);
            EXPECT_EQ(size, ::write(fd, prodData+offset, size));
        }
        EXPECT_EQ(0, ::close(fd));

        // Create the publishing node
        Tracker tracker{};
        auto    pubNode = PubNode::create(tracker, pubP2pAddr, maxPeers, 60, mcastAddr,
                ifaceAddr, 1, pubRoot, SEG_SIZE, maxOpenFiles, 3600, feedName);
        const auto pubP2pSrvrAddr = pubNode->getP2pSrvrAddr();
        Thread     pubThread(&NodeTest::runNode, this, pubNode);

        /*
         * The SubNode determines the pathname of the directory that contains information on the
         * last, successfully-processed data-product, which the SubNode uses to construct the
         * Disposer. During that construction, however, it's too late for this unit-test to add
         * pattern/actions to the Disposer. Consequently, the SubNode uses a factory method to
         * create the Disposer and, in this case, that factory is a lambda which adds the
         * unit-test pattern/action entry.
         */

        // Create disposer factory-method
        Disposer::Factory dispoFact = [&] (const String& timeTemplate) {
            Disposer      disposer{timeTemplate, 0}; // 0 => No file descriptors kept open
            Pattern       incl(".*");
            Pattern       excl{};  // Exclude nothing
            String        procTemplate(testRoot + "/processed/$&"); // Product-name as pathname
            FileTemplate  fileTemplate(procTemplate, false); // false => don't keep open
            PatternAction patAct(incl, excl, fileTemplate);
            disposer.add(patAct);
            return disposer;
        };

        // Create the subscribing node
        subInfo.tracker.insert(pubP2pSrvrAddr);
        auto peerConnSrvr = PeerConnSrvr::create(subP2pAddr, 5);
        subNodePtr = SubNode::create(subInfo, ifaceAddr, peerConnSrvr, -1, maxPeers, 60,
                subRoot, maxOpenFiles, dispoFact, this);
        Thread subThread(&NodeTest::runNode, this, subNodePtr);

        // Wait for subscriber to connect to publisher
        pubNode->waitForPeer();

        try {
            start = steady_clock::now();
            for (int i = 0; i < NUM_PRODS; ++i) {
                // Publish
                const auto prodName = prodBaseName + "." + std::to_string(i);
                const auto prodId = ProdId(prodName);
                prodInfo = ProdInfo{prodId, prodName, PROD_SIZE};

                pubNode->addProd(filePath, prodName);

                // Verify transmission and reception
                Lock lock{mutex};
                EXPECT_FALSE(threadEx);
                cond.wait(lock, [&]{return prodRcvd;});
                prodRcvd = false;
            }
            stop = steady_clock::now();
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex, "Couldn't verify transmission and reception");
            GTEST_FAIL();
        }

        //LOG_DEBUG("Halting subscriber thread");
        subNodePtr->halt();
        //LOG_DEBUG("Halting publisher thread");
        pubNode->halt();

        //LOG_DEBUG("Joining subscriber thread");
        subThread.join();
        //LOG_DEBUG("Joining publisher thread");
        pubThread.join();
    }
    catch (const std::exception& ex) {
        LOG_ERROR("Failure");
        throw;
    }

    const auto s = duration_cast<duration<double>>(stop - start);
    LOG_NOTE("");
    LOG_NOTE(to_string(NUM_PRODS) + " " + to_string(PROD_SIZE) + "-byte products in " +
            to_string(s.count()) + " seconds");
    LOG_NOTE("Product-rate = " + to_string(NUM_PRODS/s.count()) + " Hz");
    LOG_NOTE("Byte-rate = " + to_string(NUM_PRODS*PROD_SIZE/s.count()) + " Hz");
    LOG_NOTE("Bit-rate = " + to_string(NUM_PRODS*PROD_SIZE*8/s.count()) + " Hz");
    LOG_NOTE("");
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
  log_setLevel(LogLevel::NOTE);

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
