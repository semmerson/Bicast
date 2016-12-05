/**
 * This file tests the class `Product`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Product_test.cpp
 * @author: Steven R. Emmerson
 */


#include "Product.h"

#include <gtest/gtest.h>

namespace {

// The fixture for testing class Product.
class ProductTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    ProductTest() {
        // You can do set-up work for each test here.
    }

    virtual ~ProductTest() {
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

    // Objects declared here can be used by all tests in the test case for Product.
};

// Tests construction from product-information
TEST_F(ProductTest, ProdInfoConstruction) {
    hycast::ProdInfo info("product", 0, 2, 1); // 2 chunks of 1-byte each
    hycast::Product prod{info};
    EXPECT_EQ(info, prod.getInfo());
}

// Tests adding a chunk
TEST_F(ProductTest, AddChunk) {
    hycast::ProdInfo info("product", 0, 2, 1); // 2 chunks of 1-byte each
    hycast::Product prod{info};
    char data[1];
    hycast::ActualChunk chunk{hycast::ChunkInfo{0, 0}, data, sizeof(data)};
    EXPECT_TRUE(prod.add(chunk));
    EXPECT_FALSE(prod.add(chunk));
    EXPECT_TRUE(prod.add(hycast::ActualChunk(hycast::ChunkInfo{0, 1}, data,
            sizeof(data))));
    EXPECT_TRUE(prod.isComplete());
}

// Tests adding an inconsistent chunk
TEST_F(ProductTest, AddBadChunk) {
    hycast::ProdInfo info("product", 0, 3, 2); // 3 bytes in 2 chunks
    hycast::Product prod{info};
    char data[3];
    EXPECT_THROW(prod.add(hycast::ActualChunk(hycast::ChunkInfo(1, 0),
            data, 2)), std::invalid_argument); // Bad product-index
    EXPECT_THROW(prod.add(hycast::ActualChunk(hycast::ChunkInfo(0, 2),
            data, 2)), std::invalid_argument); // Bad chunk-index
    EXPECT_THROW(prod.add(hycast::ActualChunk(hycast::ChunkInfo(0, 0),
            data, 1)), std::invalid_argument); // First chunk too small
    EXPECT_THROW(prod.add(hycast::ActualChunk(hycast::ChunkInfo(0, 0),
            data, 3)), std::invalid_argument); // First chunk too large
    EXPECT_THROW(prod.add(hycast::ActualChunk(hycast::ChunkInfo(0, 1),
            data, 0)), std::invalid_argument); // Last chunk too small
    EXPECT_THROW(prod.add(hycast::ActualChunk(hycast::ChunkInfo(0, 1),
            data, 2)), std::invalid_argument); // Last chunk too large
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
