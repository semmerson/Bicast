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
#include "BicastProto.h"

#include <cassert>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

using namespace bicast;

/// The fixture for testing class `ProdFile`
class ProdFileTest : public ::testing::Test
{
protected:
    const std::string rootPath;
    const std::string prodName;
    const std::string absPathname;
    ProdId            prodId;
    ProdSize          prodSize;
    SegSize           segSize;
    ProdInfo          prodInfo;
    DataSegId         segId;
    char              memData[800];
    DataSeg           memSeg;

    // You can remove any or all of the following functions if its body
    // is empty.

    ProdFileTest()
        : rootPath("/tmp/ProdFile_test")
        , prodName("prod.dat")
        , absPathname(rootPath + "/" + prodName)
        , prodId{prodName}
        , prodSize{1000000}
        , segSize{sizeof(memData)}
        , prodInfo{prodId, prodName, prodSize}
        , segId(prodId, 0)
        , memData{}
        , memSeg{segId, prodSize, memData}
    {
        DataSeg::setMaxSegSize(segSize);
        FileUtil::rmDirTree(rootPath);
        FileUtil::ensureDir(rootPath, 0777);

        for (int i = 0; i < sizeof(memData); ++i)
            memData[i] = static_cast<char>(i);
    }

    ~ProdFileTest() noexcept {
        FileUtil::rmDirTree(rootPath);
        //rmDirTree(rootPath);
    }
};

// Tests a zero-size sending ProdFile
TEST_F(ProdFileTest, ZeroSndProdFile)
{
    ProdFile prodFile(absPathname, 0);
    EXPECT_THROW(prodFile.getData(0), InvalidArgument);
}

// Tests a valid sending ProdFile
TEST_F(ProdFileTest, ValidSndProdFile)
{
    const int fd = ::open(absPathname.data(), O_RDWR|O_CREAT, 0666);
    ASSERT_NE(-1, fd);
    const char bytes[2] = {1, 2};
    ASSERT_EQ(2, ::write(fd, bytes, sizeof(bytes)));
    ASSERT_NE(-1, ::close(fd));
    ProdFile prodFile(absPathname);
    const char* data = static_cast<const char*>(prodFile.getData(0));
    EXPECT_EQ(1, data[0]);
    EXPECT_EQ(2, data[1]);
    EXPECT_THROW(prodFile.getData(1), InvalidArgument);
}

// Tests a bad sending ProdFile
TEST_F(ProdFileTest, BadSndProdFile)
{
    try {
        const int fd = ::open(absPathname.data(), O_RDWR|O_CREAT|O_EXCL, 0666);
        ASSERT_NE(-1, fd);
        const char byte = 1;
        ASSERT_EQ(1, ::write(fd, &byte, sizeof(byte)));
        ASSERT_NE(-1, ::close(fd));
        ProdFile(absPathname, 0);
    }
    catch (const InvalidArgument& ex) {
    }
    catch (...) {
        GTEST_FAIL();
    }
}

// Tests a bad receiving ProdFile
TEST_F(ProdFileTest, BadRcvProdFile)
{
    try {
        ProdFile(absPathname, 0);
    }
    catch (const InvalidArgument& ex) {
    }
    catch (...) {
        GTEST_FAIL();
    }
}

// Tests a zero-size receiving ProdFile
TEST_F(ProdFileTest, ZeroRcvProdFile)
{
    ProdSize prodSize(0);
    ProdFile prodFile(absPathname, prodSize);
    ASSERT_TRUE(prodFile.isComplete());
}

// Tests a receiving ProdFile
TEST_F(ProdFileTest, RecvProdFile)
{
    ProdSize prodSize{static_cast<ProdSize>(2*segSize)};
    ProdInfo prodInfo(prodId, prodName, prodSize);
    ProdFile prodFile(absPathname, prodSize);
    for (int i = 0; i < 2; ++i) {
        DataSegId segId(prodId, i*segSize);
        DataSeg   memSeg(segId, prodSize, memData);
        ASSERT_TRUE(prodFile.save(memSeg));
    }
    prodFile.close();
    ASSERT_TRUE(prodFile.isComplete());
    struct stat statBuf;
    ASSERT_EQ(0, ::stat(absPathname.data(), &statBuf));
    ASSERT_EQ(prodSize, statBuf.st_size);
    const int fd = ::open(absPathname.data(), O_RDONLY);
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
