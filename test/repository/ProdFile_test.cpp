/**
 * This file tests class `ProdFile`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: ProdFile_test.cpp
 * Created On: Dec 18, 2019
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "ProdFile.h"

#include "error.h"
#include "FileUtil.h"
#include "hycast.h"

#include <cassert>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

/// The fixture for testing class `ProdFile`
class ProdFileTest : public ::testing::Test
{
protected:
    const std::string rootPath;
    int               rootFd;
    const std::string prodName;
    const std::string pathname;
    hycast::ProdIndex prodIndex;
    hycast::ProdSize  prodSize;
    hycast::SegSize   segSize;
    hycast::ProdInfo  prodInfo;
    hycast::SegId     segId;
    hycast::SegInfo   segInfo;
    char              memData[1000];
    hycast::MemSeg    memSeg;

    // You can remove any or all of the following functions if its body
    // is empty.

    ProdFileTest()
        : rootPath("/tmp/ProdFile_test")
        , rootFd(-1)
        , prodName("prod.dat")
        , pathname(rootPath + "/" + prodName)
        , prodIndex{1}
        , prodSize{1000000}
        , segSize{sizeof(memData)}
        , prodInfo{prodIndex, prodSize, "product"}
        , segId(prodIndex, 0)
        , segInfo(segId, prodSize, segSize)
        , memData{}
        , memSeg{segInfo, memData}
    {
        hycast::rmDirTree(rootPath);
        hycast::ensureDir(rootPath, 0777);
        rootFd = ::open(rootPath.data(), O_RDONLY);
        assert(rootFd != -1);

        for (int i = 0; i < sizeof(memData); ++i)
            memData[i] = static_cast<char>(i);
    }

    ~ProdFileTest() noexcept {
        if (rootFd >= 0)
            ::close(rootFd);
        hycast::rmDirTree(rootPath);
        //hycast::rmDirTree(rootPath);
    }
};

// Tests a zero-size SndProdFile
TEST_F(ProdFileTest, ZeroSndProdFile)
{
    const int fd = ::open(pathname.data(), O_RDWR|O_CREAT|O_EXCL, 0666);
    ASSERT_NE(-1, fd);
    ASSERT_NE(-1, ::close(fd));
    hycast::SndProdFile prodFile(rootFd, prodName, 0);
    EXPECT_THROW(prodFile.getData(0), hycast::InvalidArgument);
}

// Tests a valid SndProdFile
TEST_F(ProdFileTest, ValidSndProdFile)
{
    const int fd = ::open(pathname.data(), O_RDWR|O_CREAT|O_EXCL, 0666);
    ASSERT_NE(-1, fd);
    const char bytes[2] = {1, 2};
    ASSERT_EQ(2, ::write(fd, bytes, sizeof(bytes)));
    ASSERT_NE(-1, ::close(fd));
    hycast::SndProdFile prodFile(rootFd, prodName, 2);
    const char* data = static_cast<const char*>(prodFile.getData(0));
    EXPECT_EQ(1, data[0]);
    EXPECT_EQ(2, data[1]);
    EXPECT_THROW(prodFile.getData(1), hycast::InvalidArgument);
}

// Tests a bad SndProdFile
TEST_F(ProdFileTest, BadSndProdFile)
{
    try {
        const int fd = ::open(pathname.data(), O_RDWR|O_CREAT|O_EXCL, 0666);
        ASSERT_NE(-1, fd);
        const char byte = 1;
        ASSERT_EQ(1, ::write(fd, &byte, sizeof(byte)));
        ASSERT_NE(-1, ::close(fd));
        hycast::SndProdFile(rootFd, prodName, 0);
    }
    catch (const hycast::InvalidArgument& ex) {
    }
    catch (...) {
        GTEST_FAIL();
    }
}

// Tests a bad RcvProdFile
TEST_F(ProdFileTest, BadRcvProdFile)
{
    try {
        hycast::RcvProdFile(rootFd, prodIndex, 1, 0);
    }
    catch (const hycast::InvalidArgument& ex) {
    }
    catch (...) {
        GTEST_FAIL();
    }
}

// Tests a zero-size RcvProdFile
TEST_F(ProdFileTest, ZeroRcvProdFile)
{
    hycast::ProdSize    prodSize(0);
    hycast::RcvProdFile prodFile(rootFd, prodIndex, prodSize, 0);
    hycast::ProdInfo    prodInfo(prodIndex, prodSize, prodName);
    EXPECT_TRUE(prodFile.save(rootFd, prodInfo));
}

// Tests a RcvProdFile
TEST_F(ProdFileTest, RcvProdFile)
{
    hycast::ProdSize    prodSize{static_cast<hycast::ProdSize>(2*segSize)};
    hycast::ProdInfo    prodInfo(prodIndex, prodSize, prodName);
    hycast::RcvProdFile prodFile(rootFd, prodIndex, prodSize, segSize);
    ASSERT_FALSE(prodFile.save(rootFd, prodInfo));
    {
        for (int i = 0; i < 2; ++i) {
            hycast::SegId     segId(prodIndex, i*segSize);
            hycast::SegInfo   segInfo(segId, prodSize, segSize);
            hycast::MemSeg    memSeg{segInfo, memData};
            ASSERT_EQ(i == 1, prodFile.save(memSeg));
        }
    } // Closes file
    struct stat statBuf;
    ASSERT_EQ(0, ::stat(pathname.data(), &statBuf));
    ASSERT_EQ(prodSize, statBuf.st_size);
    const int fd = ::open(pathname.data(), O_RDONLY);
    ASSERT_NE(-1, fd);
    for (int i = 0; i < 2; ++i) {
        char buf[segSize];
        ASSERT_EQ(segSize, ::read(fd, buf, sizeof(buf)));
        ASSERT_EQ(0, ::memcmp(buf, memData, sizeof(buf)));
    }
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
