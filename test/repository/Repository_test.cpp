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

#include "FileUtil.h"
#include "hycast.h"
#include "Repository.h"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>

namespace {

/// The fixture for testing class `Repository`
class RepositoryTest : public ::testing::Test, public hycast::RcvRepoObs
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
        : rootPath("repo")
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
    hycast::SndRepo srcRepo(rootPath, segSize);
    hycast::RcvRepo snkRepo(rootPath, segSize, *this);
}

// Tests pathname of file
TEST_F(RepositoryTest, Pathname)
{
    hycast::SndRepo repo(rootPath, segSize);
    std::cout << "ProdId pathname: " << repo.getPathname(prodIndex) << '\n';

    const std::string& name = prodInfo.getProdName();
    std::cout << "ProdName pathname: " << repo.getPathname(name) << '\n';
}

// Tests saving just product-information
TEST_F(RepositoryTest, SaveProdInfo)
{
    int         status;
    std::string indexPath;
    {
        hycast::RcvRepo repo(rootPath, segSize, *this);
        indexPath = repo.getPathname(prodIndex);
        status = ::unlink(indexPath.data());
        if (status == -1)
            EXPECT_EQ(ENOENT, errno);
        EXPECT_FALSE(repo.getProdInfo(prodIndex));
        repo.save(prodInfo);
        auto actual = repo.getProdInfo(prodIndex);
        ASSERT_TRUE(actual);
        EXPECT_EQ(prodInfo, actual);
    } // Closes file
    struct stat statBuf;
    ASSERT_EQ(0, ::stat(indexPath.data(), &statBuf));
    ASSERT_EQ(prodSize, statBuf.st_size);
    EXPECT_EQ(0, ::unlink(indexPath.data()));
}

// Tests saving product-information and then the data
TEST_F(RepositoryTest, SaveInfoThenData)
{
    std::string indexPath;
    std::string namePath;
    {
        hycast::RcvRepo repo(rootPath, segSize, *this);

        indexPath = repo.getPathname(prodIndex);
        namePath = repo.getPathname(prodInfo.getProdName());

        ::unlink(indexPath.data());
        ::unlink(namePath.data());

        repo.save(prodInfo);
        auto actualProdInfo = repo.getProdInfo(prodIndex);
        EXPECT_EQ(prodInfo, actualProdInfo);

        repo.save(memSeg);
        auto actualSegId = repo.getMemSeg(memSeg.getSegId());
        EXPECT_EQ(memSeg, actualSegId);
    } // Closes file

    struct stat statBuf;

    ASSERT_EQ(0, ::stat(indexPath.data(), &statBuf));
    ASSERT_EQ(prodSize, statBuf.st_size);
    int fd = ::open(indexPath.data(), O_RDONLY);
    ASSERT_NE(-1, fd);
    char buf[segSize];
    ASSERT_EQ(segSize, ::read(fd, buf, sizeof(buf)));
    ASSERT_EQ(0, ::memcmp(buf, memData, sizeof(buf)));
    ASSERT_EQ(0, ::close(fd));
    ::unlink(indexPath.data());

    ASSERT_EQ(0, ::stat(namePath.data(), &statBuf));
    ASSERT_EQ(prodSize, statBuf.st_size);
    fd = ::open(namePath.data(), O_RDONLY);
    ASSERT_NE(-1, fd);
    ASSERT_EQ(segSize, ::read(fd, buf, sizeof(buf)));
    ASSERT_EQ(0, ::memcmp(buf, memData, sizeof(buf)));
    ASSERT_EQ(0, ::close(fd));
    ::unlink(namePath.data());
}

// Tests saving product-data and then product-information
TEST_F(RepositoryTest, SaveDataThenInfo)
{
    std::string indexPath;
    std::string namePath;
    {
        hycast::RcvRepo repo(rootPath, segSize, *this);
        indexPath = repo.getPathname(prodIndex);
        namePath = repo.getPathname(prodInfo.getProdName());
        ::unlink(indexPath.data());
        ::unlink(namePath.data());
        repo.save(prodInfo);
        repo.save(memSeg);
    } // Closes file

    struct stat statBuf;

    ASSERT_EQ(0, ::stat(indexPath.data(), &statBuf));
    ASSERT_EQ(prodSize, statBuf.st_size);
    int fd = ::open(indexPath.data(), O_RDONLY);
    ASSERT_NE(-1, fd);
    char buf[segSize];
    ASSERT_EQ(segSize, ::read(fd, buf, sizeof(buf)));
    ASSERT_EQ(0, ::memcmp(buf, memData, sizeof(buf)));
    ASSERT_EQ(0, ::close(fd));
    ::unlink(indexPath.data());

    ASSERT_EQ(0, ::stat(namePath.data(), &statBuf));
    ASSERT_EQ(prodSize, statBuf.st_size);
    fd = ::open(namePath.data(), O_RDONLY);
    ASSERT_NE(-1, fd);
    ASSERT_EQ(segSize, ::read(fd, buf, sizeof(buf)));
    ASSERT_EQ(0, ::memcmp(buf, memData, sizeof(buf)));
    ASSERT_EQ(0, ::close(fd));
    ::unlink(namePath.data());
}

// Tests creating a product and informing the repository
TEST_F(RepositoryTest, CreatProdForSending)
{
    // Create file
    hycast::SndRepo repo(rootPath, segSize);
    std::string     namePath = repo.getPathname(prodInfo.getProdName());
    int             status = ::unlink(namePath.data());
    ASSERT_TRUE(status == 0 || errno == ENOENT);
    int             fd = ::open(namePath.data(), O_WRONLY|O_CREAT|O_EXCL,
            0600);
    ASSERT_NE(-1, fd);
    ASSERT_EQ(segSize, ::write(fd, memData, segSize));
    ASSERT_EQ(0, ::close(fd));

    // Tell the repository
    repo.newProd(prodInfo.getProdName(), prodIndex);

    // Verify repository access
    struct stat statBuf;
    std::string indexPath = repo.getPathname(prodIndex);
    ASSERT_EQ(0, ::stat(indexPath.data(), &statBuf));
    ASSERT_EQ(prodSize, statBuf.st_size);
    fd = ::open(indexPath.data(), O_RDONLY);
    ASSERT_NE(-1, fd);
    char buf[segSize];
    ASSERT_EQ(segSize, ::read(fd, buf, sizeof(buf)));
    ASSERT_EQ(0, ::memcmp(buf, memData, sizeof(buf)));
    ASSERT_EQ(0, ::close(fd));

    ::unlink(indexPath.data());
    ::unlink(namePath.data());
    hycast::pruneDir(rootPath);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
