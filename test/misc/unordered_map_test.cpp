/**
 * This file tests class `UnorderedMap`.
 *
 *       File: unordered_map_test.cpp
 * Created On: Jun 8, 2017
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
