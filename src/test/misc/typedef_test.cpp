/**
 * This file tests `typedef`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: typedef_test.cpp
 * @author: Steven R. Emmerson
 */

#include <gtest/gtest.h>
#include <cstdint>

namespace {

template<typename T> class Index
{
    T value;
public:
    Index(T value) : value{value} {}
};

typedef Index<uint32_t> ProdIndex;
typedef Index<uint32_t> ChunkIndex;

// The fixture for testing typedef
class TypedefTest : public ::testing::Test {
protected:
    // Objects declared here can be used by all tests in the test case for Typedef.
    void func(
            const ProdIndex  prodIndex,
            const ChunkIndex chunkIndex)
    {}
};

// Tests use of typedef
TEST_F(TypedefTest, TypedefUse) {
    ProdIndex  prodIndex(1);
    ChunkIndex chunkIndex(2);
    /*
     * The following compiles even though the arguments are in reverse order.
     * Ergo, `typedef` defines an alias for a type rather than a new type.
     */
    func(chunkIndex, prodIndex);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
