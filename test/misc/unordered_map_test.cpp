/**
 * This file tests class `UnorderedMap`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: unordered_map_test.cpp
 * Created On: Jun 8, 2017
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include <string>
#include <unordered_map>
#include "gtest/gtest.h"

namespace {

class MappedValue
{
    int i;
public:
    //MappedValue() =delete; // Causes compilation failure
    MappedValue() : i{-1} {} // Necessary
    MappedValue(int i) : i{i} {}
};

/// The fixture for testing class `UnorderedMap`
class UnorderedMapTest : public ::testing::Test
{};

// Tests whether or not the mapped value of an unordered map must have a
// default constructor
TEST_F(UnorderedMapTest, NoDefaultConstructor)
{
    std::unordered_map<std::string, MappedValue> map{};
    // map["zero"] = MappedValue(); // Causes compilation failure. Good.
    map["one"] = MappedValue(1); // Requires `MappedValue::MappedValue()`
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
