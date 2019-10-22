/**
 * This file tests class `MemProd`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: MemProd_test.cpp
 * Created On: Sep 27, 2019
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "MemProd.h"

#include "gtest/gtest.h"

namespace {

/// The fixture for testing class `MemProd`
class MemProdTest : public ::testing::Test
{
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    MemProdTest()
    {
        // You can do set-up work for each test here.
    }

    virtual ~MemProdTest()
    {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp()
    {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    virtual void TearDown()
    {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    // Objects declared here can be used by all tests in the test case for Error.
};

// Tests construction
TEST(MemProdTest, Construction)
{
    std::string     name{"/foo/bar"};
    hycast::MemProd prod(name, 1000000, 1000);
    EXPECT_EQ(name, prod.getName());
    EXPECT_FALSE(prod.isComplete());
}

// Tests zero product size
TEST(MemProdTest, ZeroProdSize)
{
    std::string     name{"/foo/bar"};
    hycast::MemProd prod(name, 0, 1000);
    EXPECT_TRUE(prod.isComplete());
}

// Tests zero segment size
TEST(MemProdTest, ZeroSegSize)
{
    std::string name{"/foo/bar"};

    EXPECT_THROW(hycast::MemProd(name, 1000000, 0), hycast::InvalidArgument);
}

// Tests zero product size and zero segment size
TEST(MemProdTest, ZeroProdAndSegSize)
{
    std::string     name{"/foo/bar"};
    hycast::MemProd prod(name, 0, 0);
    EXPECT_TRUE(prod.isComplete());
}

// Tests accepting a segment
TEST(MemProdTest, SegmentAddition)
{
    std::string      name{"/foo/bar"};
    hycast::MemProd  prod(name, 1000, 1000);
    char             data[1000] = {};
    hycast::MemChunk chunk(hycast::ChunkId(0), 1000, data);

    prod.accept(chunk);
    EXPECT_TRUE(prod.isComplete());
}

// Tests accepting bad segments
TEST(MemProdTest, BadSegmentAddition)
{
    std::string      name{"/foo/bar"};
    hycast::MemProd  prod(name, 1000, 1000);
    char             data[1000] = {};
    hycast::MemChunk chunk(hycast::ChunkId(1), 1000, data);

    EXPECT_THROW(prod.accept(chunk), hycast::InvalidArgument);
    EXPECT_FALSE(prod.isComplete());

    chunk = hycast::MemChunk(hycast::ChunkId(0), 1, data);
    EXPECT_THROW(prod.accept(chunk), hycast::InvalidArgument);
    EXPECT_FALSE(prod.isComplete());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
