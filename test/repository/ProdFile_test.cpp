/**
 * This file tests class `ProdFile`.
 *
 *       File: ProdFile_test.cpp
 * Created On: Dec 18, 2019
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

#include "ProdFile.h"

#include "error.h"
#include "FileUtil.h"
#include "HycastProto.h"

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
    hycast::ProdId    prodId;
    hycast::ProdSize  prodSize;
    hycast::SegSize   segSize;
    hycast::ProdInfo  prodInfo;
    hycast::DataSegId segId;
    char              memData[800];
    hycast::DataSeg   memSeg;

    // You can remove any or all of the following functions if its body
    // is empty.

    ProdFileTest()
        : rootPath("/tmp/ProdFile_test")
        , rootFd(-1)
        , prodName("prod.dat")
        , pathname(rootPath + "/" + prodName)
        , prodId{prodName}
        , prodSize{1000000}
        , segSize{sizeof(memData)}
        , prodInfo{prodId, prodName, prodSize}
        , segId(prodId, 0)
        , memData{}
        , memSeg{segId, prodSize, memData}
    {
        hycast::DataSeg::setMaxSegSize(sizeof(memData));
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

// Tests a zero-size sending ProdFile
TEST_F(ProdFileTest, ZeroSndProdFile)
{
    const int fd = ::open(pathname.data(), O_RDWR|O_CREAT|O_EXCL, 0666);
    ASSERT_NE(-1, fd);
    ASSERT_NE(-1, ::close(fd));
    hycast::ProdFile prodFile(rootFd, prodName, 0);
    EXPECT_THROW(prodFile.getData(0), hycast::InvalidArgument);
}

// Tests a valid sending ProdFile
TEST_F(ProdFileTest, ValidSndProdFile)
{
    const int fd = ::open(pathname.data(), O_RDWR|O_CREAT|O_EXCL, 0666);
    ASSERT_NE(-1, fd);
    const char bytes[2] = {1, 2};
    ASSERT_EQ(2, ::write(fd, bytes, sizeof(bytes)));
    ASSERT_NE(-1, ::close(fd));
    hycast::ProdFile prodFile(rootFd, prodName, 2);
    const char* data = static_cast<const char*>(prodFile.getData(0));
    EXPECT_EQ(1, data[0]);
    EXPECT_EQ(2, data[1]);
    EXPECT_THROW(prodFile.getData(1), hycast::InvalidArgument);
}

// Tests a bad sending ProdFile
TEST_F(ProdFileTest, BadSndProdFile)
{
    try {
        const int fd = ::open(pathname.data(), O_RDWR|O_CREAT|O_EXCL, 0666);
        ASSERT_NE(-1, fd);
        const char byte = 1;
        ASSERT_EQ(1, ::write(fd, &byte, sizeof(byte)));
        ASSERT_NE(-1, ::close(fd));
        hycast::ProdFile(rootFd, prodName, 0);
    }
    catch (const hycast::InvalidArgument& ex) {
    }
    catch (...) {
        GTEST_FAIL();
    }
}

// Tests a bad receiving ProdFile
TEST_F(ProdFileTest, BadRcvProdFile)
{
    try {
        hycast::ProdFile(rootFd, prodName, 1, 0);
    }
    catch (const hycast::InvalidArgument& ex) {
    }
    catch (...) {
        GTEST_FAIL();
    }
}

// Tests a zero-size receiving ProdFile
TEST_F(ProdFileTest, ZeroRcvProdFile)
{
    hycast::ProdSize prodSize(0);
    hycast::ProdFile prodFile(rootFd, prodName, 0, prodSize);
    ASSERT_TRUE(prodFile.isComplete());
}

// Tests a receiving ProdFile
TEST_F(ProdFileTest, RecvProdFile)
{
    hycast::ProdSize prodSize{static_cast<hycast::ProdSize>(2*segSize)};
    hycast::ProdInfo prodInfo(prodId, prodName, prodSize);
    hycast::ProdFile prodFile(rootFd, prodName, segSize, prodSize);
    {
        for (int i = 0; i < 2; ++i) {
            hycast::DataSegId segId(prodId, i*segSize);
            hycast::DataSeg   memSeg(segId, prodSize, memData);
            ASSERT_TRUE(prodFile.save(memSeg));
        }
    } // Closes file
    ASSERT_TRUE(prodFile.isComplete());
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
