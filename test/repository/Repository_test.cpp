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

#include "logging.h"
#include "FileUtil.h"
#include "BicastProto.h"
#include "Repository.h"
#include "RunPar.h"

#include <chrono>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>

namespace {

using namespace bicast;
using namespace std;

/// The fixture for testing class `Repository`
class RepositoryTest : public ::testing::Test
{
protected:
    const String  testDir;
    const String  repoDir;
    String        prodName;
    const String  filePath;
    ProdId        prodId;
    char          memData[10000];
    const SegSize segSize;
    ProdSize      prodSize;
    ProdInfo      prodInfo;
    DataSegId     segId;
    DataSeg       dataSeg;
    LastProdPtr   lastReceived;

    RepositoryTest()
        : testDir("/tmp/Repository_test")
        , repoDir(testDir + "/repo")
        , prodName{"foo/bar/product.dat"}
        , filePath(testDir + "/" + prodName)
        , prodId{prodName}
        , memData{'A', 'B', 'C'}
        , segSize{sizeof(memData)}
        , prodSize{segSize}
        , prodInfo(prodId, prodName, prodSize)
        , segId(prodId, 0)
        , dataSeg(segId, prodSize, memData)
        , lastReceived{LastProd::create()} // Dummy lastProd object
    {
        DataSeg::setMaxSegSize(sizeof(memData));
        FileUtil::rmDirTree(testDir);
        FileUtil::ensureDir(FileUtil::dirname(filePath));

        RunPar::prodKeepTime = std::chrono::seconds(5);
    }

    ~RepositoryTest() {
        FileUtil::rmDirTree(testDir);
    }

public:
    void newProd(const ProdInfo actualProdInfo)
    {
        EXPECT_EQ(prodInfo, actualProdInfo);
    }

    void completed(const ProdInfo actualProdInfo)
    {
        EXPECT_EQ(prodInfo, actualProdInfo);
    }
};

#if 1
// Tests construction
TEST_F(RepositoryTest, Construction)
{
    //LOG_DEBUG("Creating publishing repository");
    PubRepo pubRepo{repoDir};
    //LOG_DEBUG("Creating subscribing repository");
    SubRepo subRepo{repoDir, lastReceived, true};
}
#endif

// Tests saving just product-information
TEST_F(RepositoryTest, SaveProdInfo)
{
    SubRepo repo(repoDir, lastReceived, true);
    ASSERT_FALSE(repo.getProdInfo(prodId));
    ASSERT_TRUE(repo.save(prodInfo));
    auto actual = repo.getProdInfo(prodId);
    EXPECT_TRUE(actual);
    EXPECT_EQ(prodInfo, actual);
}

#if 1
// Tests saving product-information and then the data
TEST_F(RepositoryTest, SaveInfoThenData)
{
    SubRepo repo(repoDir, lastReceived, true);

    ASSERT_TRUE(repo.save(prodInfo));
    ASSERT_TRUE(repo.save(dataSeg));

    auto prodInfo = repo.getNextProd().getProdInfo();
    ASSERT_EQ(true, prodInfo);
    EXPECT_EQ(this->prodInfo, prodInfo);

    auto actual = repo.getDataSeg(segId);
    ASSERT_TRUE(actual);
    ASSERT_EQ(dataSeg, actual);
}

// Tests saving product-data and then product-information
TEST_F(RepositoryTest, SaveDataThenInfo)
{
    SubRepo repo(repoDir, lastReceived, true);

    ASSERT_TRUE(repo.save(dataSeg));
    ASSERT_TRUE(repo.save(prodInfo));

    auto prodInfo = repo.getNextProd();
    ASSERT_EQ(true, prodInfo);
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
    PubRepo repo(repoDir);
    const auto repoProdPath = repoDir + '/' + prodName;
    FileUtil::ensureParent(repoProdPath);
    FileUtil::hardLink(filePath, repoProdPath);

    // Verify repository access
    try {
        auto repoProdInfo = repo.getNextProd().getProdInfo();
        ASSERT_TRUE(prodInfo.getId() == repoProdInfo.getId());
        ASSERT_TRUE(prodInfo.getName() == repoProdInfo.getName());
        ASSERT_TRUE(prodInfo.getSize() == repoProdInfo.getSize());
        auto repoDataSeg = repo.getDataSeg(segId);
        ASSERT_TRUE(repoDataSeg);
        ASSERT_TRUE(dataSeg == repoDataSeg);
    }
    catch (const exception& ex) {
        LOG_ERROR(ex, "Couldn't verify repository access");
        GTEST_FAIL();
    }
}

// Tests subtracting product IDs from what the repository has.
TEST_F(RepositoryTest, Subtract)
{
    SubRepo  repo(repoDir, lastReceived, true);
    ProdIdSet other{0};
    ProdIdSet prodIds{};

    prodIds = repo.subtract(other); // empty - empty -> empty
    EXPECT_EQ(0, prodIds.size());

    other.insert(prodId);
    prodIds = repo.subtract(other); // empty - prodId -> empty
    EXPECT_EQ(0, prodIds.size());

    ASSERT_TRUE(repo.save(prodInfo));
    ASSERT_TRUE(repo.save(dataSeg));
    prodIds = repo.subtract(other); // prodId - prodId -> empty
    EXPECT_EQ(0, prodIds.size());

    other.clear();
    prodIds = repo.subtract(other); // prodId - empty -> prodId
    EXPECT_EQ(1, prodIds.size());
    EXPECT_EQ(prodId, *prodIds.begin());
}

// Tests getting the set of complete product identifiers
TEST_F(RepositoryTest, getProdIds)
{
    SubRepo repo(repoDir, lastReceived, true);

    auto prodIds = repo.getProdIds(); // empty
    EXPECT_EQ(0, prodIds.size());

    ASSERT_TRUE(repo.save(prodInfo));
    ASSERT_TRUE(repo.save(dataSeg));
    prodIds = repo.getProdIds(); // prodId
    EXPECT_EQ(1, prodIds.size());
    EXPECT_EQ(prodId, *prodIds.begin());
}

TEST_F(RepositoryTest, Performance)
{
    SubRepo repo(repoDir, lastReceived, true);

    const auto     numProds = 10000;
    const ProdSize prodSize = 10*segSize;
    const auto     start = chrono::steady_clock::now();

    for (int i = 0; i < numProds; ++i) {
        prodName = to_string(i);
        auto prodId = ProdId(prodName);
        auto prodInfo = ProdInfo(prodId, prodName, prodSize);

        ASSERT_TRUE(repo.save(prodInfo));

        for (SegOffset offset = 0; offset < prodSize; offset += segSize) {
            auto segId = DataSegId(prodId, offset);
            auto dataSeg = DataSeg(segId, prodSize, memData);

            ASSERT_TRUE(repo.save(dataSeg));
        }
    }

    const auto stop = chrono::steady_clock::now();
    const auto s = chrono::duration_cast<chrono::duration<double>>(stop - start);
    LOG_NOTE(to_string(numProds) + " " + to_string(prodSize) + "-byte products in " +
            to_string(s.count()) + " seconds");
    LOG_NOTE("Product-rate = " + to_string(numProds/s.count()) + " Hz");
    LOG_NOTE("Byte-rate = " + to_string(numProds*prodSize/s.count()) + " Hz");
    LOG_NOTE("Bit-rate = " + to_string(numProds*prodSize*8/s.count()) + " Hz");
}
#endif

}  // namespace

int main(int argc, char **argv) {
  RunPar::init(argc, argv);
  log_setName(::basename(argv[0]));
  log_setLevel(LogLevel::NOTE);
  set_terminate(&bicast::terminate);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
