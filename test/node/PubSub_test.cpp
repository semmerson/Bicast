/**
 * This file tests classes `Sender` and `Receiver`.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: PubSub_test.cpp
 * Created On: Jan 3, 2020
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "FileUtil.h"
#include "hycast.h"
#include "Peer.h"
#include <condition_variable>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <main/node/Publisher.h>
#include <main/node/Subscriber.h>
#include <mutex>
#include <thread>
#include <unistd.h>

namespace {

/// The fixture for testing classes `Sender` and `Receiver`
class PubSubTest : public ::testing::Test, public hycast::PeerChngObs,
        public hycast::SubRepoObs
{
protected:
    std::mutex                mutex;
    std::condition_variable   cond;
    bool                      subConnected;
    bool                      prodReceived;
    char                      memData[1000];
    const hycast::SegSize     segSize;
    const std::string         prodName;
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
    hycast::PubRepo           pubRepo;
    hycast::SubRepo           subRepo;
    hycast::SrcMcastAddrs     srcMcastInfo;
    hycast::ServerPool        p2pSrvrPool;
    int                       numPeers;

    PubSubTest()
        : mutex()
        , cond()
        , subConnected{false}
        , prodReceived{false}
        , memData{}
        , segSize{sizeof(memData)}
        , prodIndex{1}
        , prodSize{5000}
        , prodName{"foo/bar/product.dat"}
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
        , portPool(38800, 24) // NB: Linux Dynamic port numbers
        , maxPeers{3}
        , pubP2pInfo{.sockAddr=pubSockAddr, .portPool=portPool,
                .listenSize=listenSize, .maxPeers=maxPeers}
        , subP2pInfo{.sockAddr=subSockAddr, .portPool=portPool,
                .listenSize=listenSize, .maxPeers=maxPeers}
        , pubRepo("pubRepo", segSize)
        , subRepo("subRepo", segSize, *this)
        , srcMcastInfo{.grpAddr=grpSockAddr, .srcAddr=pubInetAddr}
        , p2pSrvrPool{std::set<hycast::SockAddr>{pubSockAddr}}
        , numPeers{0}
    {
        ::memset(memData, 0xbd, segSize);
    }

    virtual ~PubSubTest()
    {
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp()
    {
        hycast::deleteDir(pubRepo.getRootDir());
        hycast::deleteDir(subRepo.getRootDir());
    }

    virtual void TearDown()
    {
    }

public:
    void added(hycast::Peer& peer)
    {
        LOG_INFO("Added peer %s", peer.to_string().data());
        std::lock_guard<std::mutex> guard{mutex};
        if (++numPeers == 2) {
            subConnected = true;
            cond.notify_one();
        }
    }

    void removed(hycast::Peer& peer)
    {
        LOG_INFO("Removed peer %s", peer.to_string().data());
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
TEST_F(PubSubTest, PublisherConstruction)
{
    hycast::Publisher(pubP2pInfo, grpSockAddr, pubRepo, *this);
}

// Tests subscriber construction
TEST_F(PubSubTest, SubscriberConstruction)
{
    hycast::Subscriber(srcMcastInfo, subP2pInfo, p2pSrvrPool, subRepo, *this);
}

// Tests sending
TEST_F(PubSubTest, Sending)
{
    try {
        // Construct publisher
        hycast::Publisher publisher(pubP2pInfo, grpSockAddr, pubRepo, *this);
        std::thread       pubThread(&hycast::Publisher::operator(), &publisher);

        // Construct subscriber
        hycast::Subscriber subscriber(srcMcastInfo, subP2pInfo, p2pSrvrPool,
                subRepo, *this);
        std::thread        subThread(&hycast::Subscriber::operator(), &subscriber);

        // Wait until the subscriber connects
        {
            std::unique_lock<std::mutex> lock{mutex};
            while (!subConnected)
                cond.wait(lock);
        }

        //::usleep(0);

        // Create product-file
        std::string    namePath = pubRepo.getPathname(prodName);
        int            status = ::unlink(namePath.data());
        EXPECT_TRUE(status == 0 || errno == ENOENT);
        const std::string  dirPath = hycast::dirPath(namePath);
        hycast::ensureDir(dirPath, 0700);
        int            fd = ::open(namePath.data(), O_WRONLY|O_CREAT|O_EXCL,
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
        publisher.send(prodName, prodIndex);

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
