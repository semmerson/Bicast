/**
 * This file tests class `Sender`.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: Sender_test.cpp
 * Created On: Jan 3, 2020
 *     Author: Steven R. Emmerson
 */
#include "config.h"
#include "Sender.h"

#include "FileUtil.h"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

namespace {

/// The fixture for testing class `Sender`
class SenderTest : public ::testing::Test
{
protected:
    hycast::SockAddr      grpAddr;
    hycast::SockAddr      sndrAddr;
    hycast::SockAddr      rcvrAddr;
    int                   listenSize;
    hycast::PortPool      portPool;
    int                   maxPeers;
    hycast::SrcRepo       repo;
    hycast::ProdIndex     prodIndex;
    char                  memData[1000];
    const hycast::SegSize segSize;
    hycast::ProdSize      prodSize;
    hycast::ProdInfo      prodInfo;
    hycast::SegId         segId;
    hycast::SegInfo       segInfo;
    hycast::MemSeg        memSeg;

    SenderTest()
        : grpAddr("232.1.1.1:3880")
        , sndrAddr{"localhost:3880"}
        , rcvrAddr{"localhost:3881"} // NB: Not a Linux dynamic port number
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

    virtual ~SenderTest()
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
TEST_F(SenderTest, Construction)
{
    hycast::Sender sender(sndrAddr, listenSize, portPool, maxPeers, grpAddr,
            repo);
}

// Tests sending
TEST_F(SenderTest, Sending)
{
    // Create product-file
    hycast::Sender sender(sndrAddr, listenSize, portPool, maxPeers, grpAddr,
            repo);
    std::string        namePath = repo.getPathname(prodInfo.getName());
    int                status = ::unlink(namePath.data());
    ASSERT_TRUE(status == 0 || errno == ENOENT);
    const std::string  dirPath = hycast::dirPath(namePath);
    hycast::ensureDir(dirPath, 0700);
    int                fd = ::open(namePath.data(), O_WRONLY|O_CREAT|O_EXCL,
            0600);
    ASSERT_NE(-1, fd);
    ASSERT_EQ(segSize, ::write(fd, memData, segSize));
    ASSERT_EQ(0, ::close(fd));

    // Tell repository
    repo.newProd(prodInfo.getName(), prodIndex);

    hycast::ProdInfo repoProdInfo = repo.getProdInfo(prodIndex);
    ASSERT_TRUE(repoProdInfo);
    sender.send(repoProdInfo);

    auto repoSeg = repo.getMemSeg(segId);
    ASSERT_TRUE(repoSeg);
    sender.send(repoSeg);

    ::unlink(namePath.data());
    ::unlink(repo.getPathname(prodIndex).data());
    hycast::pruneDir(repo.getRootDir());
}

}  // namespace

int main(int argc, char **argv) {
  hycast::log_setName(::basename(argv[0]));
  hycast::log_setLevel(hycast::LOG_LEVEL_DEBUG);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
