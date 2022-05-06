/**
 * This file tests class `Repository`.
 *
 *       File: Repository_test.cpp
 * Created On: Dec 23, 2019
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

#include "error.h"
#include "FileUtil.h"
#include "HycastProto.h"
#include "Repository.h"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>

namespace {

/// The fixture for testing class `Repository`
class RepositoryTest : public ::testing::Test
{
protected:
    const std::string     testDir;
    const std::string     rootDir;
    std::string           prodName;
    const std::string     filePath;
    hycast::ProdIndex     prodIndex;
    char                  memData[1450];
    const hycast::SegSize segSize;
    hycast::ProdSize      prodSize;
    hycast::ProdInfo      prodInfo;
    hycast::DataSegId     segId;
    hycast::DataSeg       dataSeg;

    RepositoryTest()
        : testDir("/tmp/Repository_test")
        , rootDir(testDir + "/repo")
        , prodName{"foo/bar/product.dat"}
        , filePath(testDir + "/" + prodName)
        , prodIndex{1} // Index of first product in empty repository is 1
        , memData{'A', 'B', 'C'}
        , segSize{sizeof(memData)}
        , prodSize{segSize}
        , prodInfo(prodIndex, prodName, prodSize)
        , segId(prodIndex, 0)
        , dataSeg(segId, prodSize, memData)
    {
        hycast::DataSeg::setMaxSegSize(sizeof(memData));
        hycast::rmDirTree(testDir);
        hycast::ensureDir(hycast::dirPath(filePath));
    }

    ~RepositoryTest() {
        hycast::rmDirTree(testDir);
    }

public:
    void newProd(const hycast::ProdInfo actualProdInfo)
    {
        EXPECT_EQ(prodInfo, actualProdInfo);
    }

    void completed(const hycast::ProdInfo actualProdInfo)
    {
        EXPECT_EQ(prodInfo, actualProdInfo);
    }
};

// Tests construction
TEST_F(RepositoryTest, Construction)
{
    hycast::PubRepo pubRepo{rootDir, segSize, 5};
    hycast::SubRepo subRepo{rootDir, segSize, 5};
}

// Tests saving just product-information
TEST_F(RepositoryTest, SaveProdInfo)
{
    hycast::SubRepo repo(rootDir, segSize, 5);
    ASSERT_FALSE(repo.getProdInfo(prodIndex));
    ASSERT_TRUE(repo.save(prodInfo));
    auto actual = repo.getProdInfo(prodIndex);
    ASSERT_TRUE(actual);
    EXPECT_EQ(prodInfo, actual);
}

// Tests saving product-information and then the data
TEST_F(RepositoryTest, SaveInfoThenData)
{
    hycast::SubRepo repo(rootDir, segSize, 5);

    ASSERT_TRUE(repo.save(prodInfo));
    ASSERT_TRUE(repo.save(dataSeg));

    auto prodInfo = repo.getNextProd();
    ASSERT_TRUE(prodInfo);
    EXPECT_EQ(RepositoryTest::prodInfo, prodInfo);

    auto actual = repo.getDataSeg(segId);
    ASSERT_TRUE(actual);
    ASSERT_EQ(dataSeg, actual);
}

// Tests saving product-data and then product-information
TEST_F(RepositoryTest, SaveDataThenInfo)
{
    hycast::SubRepo repo(rootDir, segSize, 5);

    ASSERT_TRUE(repo.save(dataSeg));
    ASSERT_TRUE(repo.save(prodInfo));

    auto prodInfo = repo.getNextProd();
    ASSERT_TRUE(prodInfo);
    EXPECT_EQ(RepositoryTest::prodInfo, prodInfo);

    auto actual = repo.getDataSeg(dataSeg.getId());
    ASSERT_TRUE(actual);
    ASSERT_EQ(dataSeg, actual);
}

// Tests creating a product and informing a publisher's repository
TEST_F(RepositoryTest, CreatProdForSending)
{
    // Create file
    int fd = ::open(filePath.data(), O_WRONLY|O_CREAT|O_EXCL, 0600);
    ASSERT_NE(-1, fd);
    ASSERT_EQ(segSize, ::write(fd, RepositoryTest::memData, segSize));
    ASSERT_EQ(0, ::close(fd));

    // Create the publisher's repository and tell it about the file
    hycast::PubRepo repo(rootDir, segSize, 5);
    repo.link(filePath, prodInfo.getName());

    // Verify repository access
    try {
        auto repoProdInfo = repo.getNextProd();
        ASSERT_TRUE(prodInfo == repoProdInfo);
        auto repoDataSeg = repo.getDataSeg(segId);
        ASSERT_TRUE(repoDataSeg);
        ASSERT_TRUE(dataSeg == repoDataSeg);
    }
    catch (const std::exception& ex) {
        LOG_ERROR(ex, "Couldn't verify repository access");
        GTEST_FAIL();
    }
}
#if 0
#endif

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
