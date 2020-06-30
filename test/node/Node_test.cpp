/**
 * This file tests class `Node`
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: Node_test.cpp
 * Created On: Jan 3, 2020
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "FileUtil.h"
#include "hycast.h"
#include "Node.h"

#include <condition_variable>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <main/node/Publisher.h>
#include <main/node/Subscriber.h>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

/// The fixture for testing class `Node`
class NodeTest
    : public ::testing::Test
    , public hycast::Node
    , public hycast::P2pObs
{
protected:
    std::mutex                mutex;
    std::condition_variable   cond;
    bool                      subConnected;
    bool                      prodReceived;
    char                      memData[1000];
    const hycast::SegSize     segSize;
    const std::string         prodName;
    const std::string         prodPath;
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
    hycast::PortPool          portPool;
    const int                 maxPeers;
    hycast::P2pInfo           pubP2pInfo;
    hycast::P2pInfo           subP2pInfo;
    std::string               pubRepoName;
    std::string               subRepoName;
    hycast::SrcMcastAddrs     srcMcastInfo;
    hycast::ServerPool        p2pSrvrPool;
    int                       numPeers;

    NodeTest()
        : mutex()
        , cond()
        , subConnected{false}
        , prodReceived{false}
        , memData{}
        , segSize{sizeof(memData)}
        , prodIndex{1}
        , prodSize{5000}
        , prodName{"foo/bar/product.dat"}
        , prodPath("/tmp/" + prodName)
        , prodInfo{prodIndex, prodSize, prodName}
        , segId(prodIndex, 0)
        , segInfo(segId, prodSize, segSize)
        , memSeg{segInfo, memData}
        , grpSockAddr("232.1.1.1:3880")
        , pubInetAddr{"192.168.174.141"}
        , pubSockAddr{pubInetAddr, 3880}
        , subInetAddr{"192.168.174.141"}
        , subSockAddr{subInetAddr, 3881} // NB: Not a Linux dynamic port number
        , subSockAddr{subInetAddr, 3881} // NB: Not a Linux dynamic port number
        , listenSize{0}
        , portPool(38800, 24) // NB: Linux Dynamic port numbers
        , maxPeers{3}
        , pubP2pInfo{.sockAddr=pubSockAddr, .portPool=portPool,
                .listenSize=listenSize, .maxPeers=maxPeers}
        , subP2pInfo{.sockAddr=subSockAddr, .portPool=portPool,
                .listenSize=listenSize, .maxPeers=maxPeers}
        , subRepoName("pubRepo")
        , subRepoName("subRepo")
        , srcMcastInfo{.grpAddr=grpSockAddr, .srcAddr=pubInetAddr}
        , p2pSrvrPool{std::set<hycast::SockAddr>{pubSockAddr}}
        , numPeers{0}
    {
        ::memset(memData, 0xbd, segSize);
    }

    virtual ~NodeTest() {
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp() {
        hycast::deleteDir(pubRepoName);
        hycast::deleteDir(subRepoName);
    }

    virtual void TearDown() {
    }

public:
    void peerAdded(hycast::Peer peer)
    {
        LOG_INFO("Added peer %s", peer.to_string().data());
        std::lock_guard<std::mutex> guard{mutex};
        if (++numPeers == 2) {
            subConnected = true;
            cond.notify_one();
        }
    }

    void completed(const hycast::ProdInfo& actual)
    {
        EXPECT_EQ(prodInfo, actual);

        std::lock_guard<std::mutex> guard{mutex};
        prodReceived = true;
        cond.notify_one();
    }
};

// Tests publisher construction
TEST_F(NodeTest, PublisherConstruction) {
    hycast::Node(pubP2pInfo, grpSockAddr, pubRepoName, *this);
}

// Tests subscriber construction
TEST_F(NodeTest, SubscriberConstruction)
{
    hycast::Node(srcMcastInfo, subP2pInfo, p2pSrvrPool, subRepoName, segSize,
            *this);
}

// Tests sending
TEST_F(NodeTest, Sending)
{
    try {
        // Construct publisher
        hycast::Node publisher(pubP2pInfo, grpSockAddr, pubRepoName, *this);
        std::thread  pubThread(&hycast::Node::operator(), &publisher);

        // Construct subscriber
        hycast::Node subscriber(srcMcastInfo, subP2pInfo, p2pSrvrPool,
                subRepoName, segSize, *this);
        std::thread  subThread(&hycast::Node::operator(), &subscriber);

        // Wait until the subscriber's peer connects
        {
            std::unique_lock<std::mutex> lock{mutex};
            while (!subConnected)
                cond.wait(lock);
        }

        // Create product-file
        int                status = ::unlink(prodPath.data());
        EXPECT_TRUE(status == 0 || errno == ENOENT);
        const std::string  dirPath = hycast::dirPath(prodPath);
        hycast::ensureDir(dirPath, 0700);
        int                fd = ::open(prodPath.data(), O_WRONLY|O_CREAT|O_EXCL,
                0600);
        EXPECT_NE(-1, fd);
        for (hycast::ProdSize offset = 0; offset < prodSize; offset += segSize) {
            const hycast::SegSize size = (offset + segSize > prodSize)
                    ? prodSize - offset
                    : segSize;
            EXPECT_EQ(size, ::write(fd, memData, size));
        }
        EXPECT_EQ(0, ::close(fd));

        // Tell sender
        publisher.link(prodName, prodPath);

        // Wait until product is completely received
        {
            std::unique_lock<std::mutex> lock{mutex};
            while (!prodReceived)
                cond.wait(lock);
        }

        subscriber.halt();
        publisher.halt(); // Closes connection with subscriber's peer
        subThread.join();
        pubThread.join();
        //::unlink(namePath.data());
        //::unlink(pubRepo.getPathname(prodIndex).data());
        //hycast::pruneDir(pubRepo.getRootDir());
    }
    catch (const std::exception& ex) {
        LOG_ERROR("Failure");
        throw;
    }
}

}  // namespace

int main(int argc, char **argv) {
  hycast::log_setName(::basename(argv[0]));
  //hycast::log_setLevel(hycast::LOG_LEVEL_DEBUG);

  /*
   * Ignore SIGPIPE so that writing to a closed socket doesn't terminate the
   * process (the return-value from write() is always checked).
   */
  struct sigaction sigact;
  sigact.sa_handler = SIG_IGN;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = 0;
  (void)sigaction(SIGPIPE, &sigact, NULL);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
