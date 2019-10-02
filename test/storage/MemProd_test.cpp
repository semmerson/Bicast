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

// Tests default construction
TEST(MemProdTest, DefaultConstruction)
{
    hycast::MemProd MemProd();
}

// Tests accepting a product-header
TEST(MemProdTest, AcceptProdHead)
{
    hycast::MemProd MemProd();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
