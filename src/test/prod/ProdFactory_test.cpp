/**
 * This file tests the class `ProdFactory`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdFactory_test.cpp
 * @author: Steven R. Emmerson
 */


#include "Chunk.h"
#include "HycastTypes.h"
#include "ProdInfo.h"
#include "ProdFactory.h"

#include <cstring>
#include <gtest/gtest.h>

namespace {

// The fixture for testing class ProdFactory.
class ProdFactoryTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    ProdFactoryTest() {
        // You can do set-up work for each test here.
    }

    virtual ~ProdFactoryTest() {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp() {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    virtual void TearDown() {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    // Objects declared here can be used by all tests in the test case for ProdFactory.
    const hycast::ProdIndex_t prodIndex{1};
    const hycast::ProdSize    prodSize{3};
    const hycast::ChunkSize   chunkSize{2};
    const hycast::ProdInfo    prodInfo{"product", prodIndex, prodSize, chunkSize};
    char                      data[3] = {'a', 'b', 'c'};
    hycast::ActualChunk       chunk0{hycast::ChunkInfo(prodIndex, 0), data,
        chunkSize};
    hycast::ActualChunk       chunk1{hycast::ChunkInfo(prodIndex, 1),
        data+chunkSize, 1};
};

// Tests default construction
TEST_F(ProdFactoryTest, DefaultConstruction) {
    hycast::ProdFactory prodFac{};
}

// Tests adding an orphan chunk of data
TEST_F(ProdFactoryTest, AddOrphanChunk) {
    hycast::Product prod;
    EXPECT_EQ(hycast::ProdFactory::AddStatus::NO_SUCH_PRODUCT,
            hycast::ProdFactory().add(chunk0, &prod));
}

// Tests adding and deleting
TEST_F(ProdFactoryTest, AdditionAndDeletion) {
    hycast::ProdFactory prodFac{};
    EXPECT_FALSE(prodFac.erase(prodIndex));
    EXPECT_TRUE(prodFac.add(prodInfo));
    EXPECT_FALSE(prodFac.add(prodInfo));
    hycast::Product prod;
    EXPECT_EQ(hycast::ProdFactory::AddStatus::PRODUCT_IS_INCOMPLETE,
            prodFac.add(chunk0, &prod));
    EXPECT_EQ(hycast::ProdFactory::AddStatus::PRODUCT_IS_COMPLETE,
            prodFac.add(chunk1, &prod));
    EXPECT_EQ(sizeof(data), prod.getInfo().getSize());
    EXPECT_EQ(0, ::memcmp(data, prod.getData(), sizeof(data)));
    EXPECT_FALSE(prodFac.add(prodInfo));
    EXPECT_TRUE(prodFac.erase(prodIndex));
}

#if 0
// Tests adding a chunk of data but not completing a product
TEST_F(ProdFactoryTest, AddIncompleteChunk) {
    hycast::Product prod;
    EXPECT_EQ(hycast::ProdFactory::AddStatus::NO_SUCH_PRODUCT,
            hycast::ProdFactory().add(chunk0, &prod));
}
#endif

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
