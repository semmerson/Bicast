/**
 * This file tests the pImpl idiom.
 */
#include "gtest/gtest.h"
#include <memory>
#include <iostream>

namespace {

class Impl
{
public:
    ~Impl() {
        std::cerr << "~Impl() called" << '\n';
    }
};

class Ref
{
    std::shared_ptr<Impl> pImpl;

public:
    Ref()
        : pImpl{new Impl()}
    {}
};

/// The fixture for testing class `Pimpl`
class PimplTest : public ::testing::Test
{
protected:
    Ref ref;

    PimplTest()
        : ref()
    {}
    virtual ~PimplTest() {}
    virtual void SetUp() {}
    virtual void TearDown() {}

public:
    void assignRef()
    {
        ref = Ref();
    }
};

/// Tests pImpl destruction
TEST_F(PimplTest, PimplDestruction)
{
    ref = Ref();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
