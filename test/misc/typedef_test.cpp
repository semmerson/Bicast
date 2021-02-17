/**
 * This file tests `typedef`.
 *
 *   @file: typedef_test.cpp
 * @author: Steven R. Emmerson
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
