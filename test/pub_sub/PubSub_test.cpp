/**
 * This file tests classes `Sender` and `Receiver`.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: SndrRcvr_test.cpp
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
#include <main/pub_sub/Publisher.h>
#include <main/pub_sub/Subscriber.h>
#include <mutex>
#include <thread>
#include <unistd.h>

namespace {

/// The fixture for testing classes `Sender` and `Receiver`
class PubSubTest : public ::testing::Test, public hycast::PeerChngObs,
        public hycast::RcvRepoObs
{
protected:
    std::mutex                mutex;
    std::condition_variable   cond;
    bool                      rcvrConnected;
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
    const hycast::InetAddr    sndrInetAddr;
    const hycast::InetAddr    rcvrInetAddr;
    const hycast::SockAddr    sndrSockAddr;
    const hycast::SockAddr    rcvrSockAddr;
    const int                 listenSize;
    hycast::PortPool          portPool;
    const int                 maxPeers;
    hycast::P2pInfo           sndrP2pInfo;
    hycast::P2pInfo           rcvrP2pInfo;
    hycast::SndRepo           sndrRepo;
    hycast::RcvRepo           rcvrRepo;
    hycast::SrcMcastAddrs      srcMcastInfo;
    hycast::ServerPool        p2pSrvrPool;
    int                       numPeers;

    PubSubTest()
        : mutex()
        , cond()
        , rcvrConnected{false}
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
        , sndrInetAddr{"192.168.174.141"}
        , sndrSockAddr{sndrInetAddr, 3880}
        , rcvrInetAddr{"192.168.174.141"}
        , rcvrSockAddr{rcvrInetAddr, 3881} // NB: Not a Linux dynamic port number
        , listenSize{0}
        , portPool(38800, 24) // NB: Linux Dynamic port numbers
        , maxPeers{3}
        , sndrP2pInfo{.sockAddr=sndrSockAddr, .portPool=portPool,
                .listenSize=listenSize, .maxPeers=maxPeers}
        , rcvrP2pInfo{.sockAddr=rcvrSockAddr, .portPool=portPool,
                .listenSize=listenSize, .maxPeers=maxPeers}
        , sndrRepo("sndrRepo", segSize)
        , rcvrRepo("rcvrRepo", segSize, *this)
        , srcMcastInfo{.grpAddr=grpSockAddr, .srcAddr=sndrInetAddr}
        , p2pSrvrPool{std::set<hycast::SockAddr>{sndrSockAddr}}
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
        hycast::deleteDir(sndrRepo.getRootDir());
        hycast::deleteDir(rcvrRepo.getRootDir());
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
            rcvrConnected = true;
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

// Tests sender construction
TEST_F(PubSubTest, SenderConstruction)
{
    hycast::Publisher(sndrP2pInfo, grpSockAddr, sndrRepo, *this);
}

// Tests receiver construction
TEST_F(PubSubTest, ReceiverConstruction)
{
    hycast::Subscriber(srcMcastInfo, rcvrP2pInfo, p2pSrvrPool, rcvrRepo, *this);
}

// Tests sending
TEST_F(PubSubTest, Sending)
{
    try {
        // Construct sender
        hycast::Publisher sender(sndrP2pInfo, grpSockAddr, sndrRepo, *this);
        std::thread    sndrThread(&hycast::Publisher::operator(), &sender);

        // Construct receiver
        hycast::Subscriber receiver(srcMcastInfo, rcvrP2pInfo, p2pSrvrPool,
                rcvrRepo, *this);
        std::thread      rcvrThread(&hycast::Subscriber::operator(), &receiver);

        // Wait until the receiver connects
        {
            std::unique_lock<std::mutex> lock{mutex};
            while (!rcvrConnected)
                cond.wait(lock);
        }

        //::usleep(0);

        // Create product-file
        std::string    namePath = sndrRepo.getPathname(prodName);
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
        sender.send(prodName, prodIndex);

        // Wait until product is completely received
        {
            std::unique_lock<std::mutex> lock{mutex};
            while (!prodReceived)
                cond.wait(lock);
        }

        receiver.halt();
        sender.halt(); // Closes connection with receiver's peer
        rcvrThread.join();
        sndrThread.join();
        //::unlink(namePath.data());
        //::unlink(sndrRepo.getPathname(prodIndex).data());
        //hycast::pruneDir(sndrRepo.getRootDir());
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
