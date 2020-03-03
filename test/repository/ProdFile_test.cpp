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
#include "hycast.h"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

/// The fixture for testing class `ProdFile`
class ProdFileTest : public ::testing::Test
{
protected:
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
        : pathname("ProdFile_test.dat")
        , prodIndex{1}
        , prodSize{1000000}
        , segSize{sizeof(memData)}
        , prodInfo{prodIndex, prodSize, "product"}
        , segId(prodIndex, 0)
        , segInfo(segId, prodSize, segSize)
        , memData{}
        , memSeg{segInfo, memData}
    {
        for (int i = 0; i < sizeof(memData); ++i)
            memData[i] = static_cast<char>(i);
    }
};

// Tests a zero-size SndProdFile
TEST_F(ProdFileTest, ZeroSndProdFile)
{
    (void)::unlink(pathname.data());
    const int fd = ::open(pathname.data(), O_RDWR|O_CREAT|O_EXCL, 0666);
    ASSERT_NE(-1, fd);
    ASSERT_NE(-1, ::close(fd));
    hycast::SndProdFile prodFile(pathname, 0);
    EXPECT_THROW(prodFile.getData(0), hycast::InvalidArgument);
    EXPECT_EQ(0, ::unlink(pathname.data()));
}

// Tests a valid SndProdFile
TEST_F(ProdFileTest, ValidSndProdFile)
{
    (void)::unlink(pathname.data());
    const int fd = ::open(pathname.data(), O_RDWR|O_CREAT|O_EXCL, 0666);
    ASSERT_NE(-1, fd);
    const char bytes[2] = {1, 2};
    ASSERT_EQ(2, ::write(fd, bytes, sizeof(bytes)));
    ASSERT_NE(-1, ::close(fd));
    hycast::SndProdFile prodFile(pathname, 2);
    const char* data = static_cast<const char*>(prodFile.getData(0));
    EXPECT_EQ(1, data[0]);
    EXPECT_EQ(2, data[1]);
    EXPECT_THROW(prodFile.getData(1), hycast::InvalidArgument);
    EXPECT_EQ(0, ::unlink(pathname.data()));
}

// Tests a bad SndProdFile
TEST_F(ProdFileTest, BadSndProdFile)
{
    (void)::unlink(pathname.data());
    try {
        const int fd = ::open(pathname.data(), O_RDWR|O_CREAT|O_EXCL, 0666);
        ASSERT_NE(-1, fd);
        const char byte = 1;
        ASSERT_EQ(1, ::write(fd, &byte, sizeof(byte)));
        ASSERT_NE(-1, ::close(fd));
        hycast::SndProdFile(pathname, 0);
    }
    catch (const hycast::InvalidArgument& ex) {
    }
    catch (...) {
        GTEST_FAIL();
    }
    EXPECT_EQ(0, ::unlink(pathname.data()));
}

// Tests a bad RcvProdFile
TEST_F(ProdFileTest, BadRcvProdFile)
{
    (void)::unlink(pathname.data());
    try {
        hycast::RcvProdFile(pathname, 1, 0);
    }
    catch (const hycast::InvalidArgument& ex) {
    }
    catch (...) {
        GTEST_FAIL();
    }
    EXPECT_EQ(-1, ::unlink(pathname.data()));
}

// Tests a zero-size RcvProdFile
TEST_F(ProdFileTest, ZeroRcvProdFile)
{
    (void)::unlink(pathname.data());
    hycast::RcvProdFile prodFile(pathname, 0, 0);
    EXPECT_TRUE(prodFile.isComplete());
    EXPECT_EQ(0, ::unlink(pathname.data()));
}

// Tests a RcvProdFile
TEST_F(ProdFileTest, RcvProdFile)
{
    (void)::unlink(pathname.data());
    hycast::ProdSize prodSize{static_cast<hycast::ProdSize>(2*segSize)};
    hycast::RcvProdFile prodFile(pathname, prodSize, segSize);
    {
        ASSERT_FALSE(prodFile.isComplete());
        for (int i = 0; i < 2; ++i) {
            hycast::SegId     segId(prodIndex, i*segSize);
            hycast::SegInfo   segInfo(segId, prodSize, segSize);
            hycast::MemSeg    memSeg{segInfo, memData};
            ASSERT_TRUE(prodFile.save(memSeg));
        }
        ASSERT_TRUE(prodFile.isComplete());
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
    EXPECT_EQ(0, ::unlink(pathname.data()));
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
