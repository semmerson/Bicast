/**
 * This file tests class `Repository`.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: Repository_test.cpp
 * Created On: Dec 23, 2019
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "FileUtil.h"
#include "hycast.h"
#include "Repository.h"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>

namespace {

/// The fixture for testing class `Repository`
class RepositoryTest : public ::testing::Test
{
protected:
    const std::string     rootPath;
    hycast::ProdIndex     prodIndex;
    char                  memData[1000];
    const hycast::SegSize segSize;
    hycast::ProdSize      prodSize;
    hycast::ProdInfo      prodInfo;
    hycast::SegId         segId;
    hycast::SegInfo       segInfo;
    hycast::MemSeg        memSeg;

    RepositoryTest()
        : rootPath("/tmp/Repository_test")
        , prodIndex{1}
        , memData{}
        , segSize{sizeof(memData)}
        , prodSize{segSize}
        , prodInfo{prodIndex, prodSize, "foo/bar/product.dat"}
        , segId(prodIndex, 0)
        , segInfo(segId, prodSize, segSize)
        , memSeg{segInfo, memData}
    {
        hycast::rmDirTree(rootPath);
        ::memset(memData, 0xbd, segSize);
    }

    ~RepositoryTest() {
        hycast::rmDirTree(rootPath);
    }

public:
    void newProd(const hycast::ProdInfo& actualProdInfo)
    {
        EXPECT_EQ(prodInfo, actualProdInfo);
    }

    void completed(const hycast::ProdInfo& actualProdInfo)
    {
        EXPECT_EQ(prodInfo, actualProdInfo);
    }
};

// Tests construction
TEST_F(RepositoryTest, Construction)
{
    hycast::PubRepo pubRepo(rootPath, segSize);
    hycast::SubRepo subRepo(rootPath, segSize);
}

// Tests saving just product-information
TEST_F(RepositoryTest, SaveProdInfo)
{
    hycast::SubRepo repo(rootPath, segSize);
    ASSERT_FALSE(repo.getProdInfo(prodIndex));
    ASSERT_FALSE(repo.save(prodInfo));
    hycast::ProdInfo actual = repo.getProdInfo(prodIndex);
    ASSERT_TRUE(actual);
    EXPECT_EQ(prodInfo, actual);
}

// Tests saving product-information and then the data
TEST_F(RepositoryTest, SaveInfoThenData)
{
    hycast::SubRepo repo(rootPath, segSize);

    ASSERT_FALSE(repo.save(prodInfo));
    ASSERT_TRUE(repo.save(memSeg));

    auto prodInfo = repo.getCompleted();
    ASSERT_TRUE(prodInfo);
    EXPECT_EQ(this->prodInfo, prodInfo);

    auto actualMemSeg = repo.getMemSeg(memSeg.getSegId());
    ASSERT_TRUE(actualMemSeg);
    ASSERT_EQ(memSeg, actualMemSeg);
}

// Tests saving product-data and then product-information
TEST_F(RepositoryTest, SaveDataThenInfo)
{
    hycast::SubRepo repo(rootPath, segSize);

    ASSERT_FALSE(repo.save(memSeg));
    ASSERT_TRUE(repo.save(prodInfo));

    auto prodInfo = repo.getCompleted();
    ASSERT_TRUE(prodInfo);
    EXPECT_EQ(RepositoryTest::prodInfo, prodInfo);

    auto actualMemSeg = repo.getMemSeg(memSeg.getSegId());
    ASSERT_TRUE(actualMemSeg);
    ASSERT_EQ(RepositoryTest::memSeg, actualMemSeg);
}

// Tests creating a product and informing a publisher's repository
TEST_F(RepositoryTest, CreatProdForSending)
{
    // Create file
    const std::string pathname("/tmp/Repository_test.data");
    int               status = ::unlink(pathname.data());
    ASSERT_TRUE(status == 0 || errno == ENOENT);
    int               fd = ::open(pathname.data(),
            O_WRONLY|O_CREAT|O_EXCL, 0600);
    ASSERT_NE(-1, fd);
    ASSERT_EQ(segSize, ::write(fd, memData, segSize));
    ASSERT_EQ(0, ::close(fd));

    // Create the publisher's repository and tell it about the file
    hycast::PubRepo repo(rootPath, segSize);
    repo.link(pathname, prodInfo.getProdName());

    // Verify repository access
    try {
        auto prodIndex = repo.getNextProd();
        auto prodInfo = repo.getProdInfo(prodIndex);
        ASSERT_TRUE(RepositoryTest::prodInfo == prodInfo);
        auto memSeg = repo.getMemSeg(RepositoryTest::segId);
        ASSERT_EQ(RepositoryTest::memSeg, memSeg);
    }
    catch (const std::exception& ex) {
        LOG_ERROR(ex, "Couldn't verify repository access");
        GTEST_FAIL();
    }

    ::unlink(pathname.data());
}
#if 0
#endif

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
