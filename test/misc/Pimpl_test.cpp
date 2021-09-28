/**
 * This file tests the pImpl idiom.
 */
#include "Pimpl_test.h"

#include <gtest/gtest.h>
#include <iostream>

namespace {

class Ref::Impl
{
    Ref& ref;

public:
    Impl(Ref& ref)
        : ref(ref)
    {}

    ~Impl()
    {
        std::cerr << "~Impl() called" << '\n';
    }

    void func() {
        std::cerr << "Impl::func() called" << '\n';
    }
};

Ref::Ref()
    : pImpl(new Impl(*this))
{}

void Ref::func()
{
    pImpl->func();
}

/// The fixture for testing class `Pimpl`
class PimplTest : public ::testing::Test
{
protected:
    Ref ref;

    PimplTest()
        : ref()
    {}
    ~PimplTest() {}
    void SetUp() {}
    void TearDown() {}

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

/// Tests calling pImpl
TEST_F(PimplTest, CallingPimpl)
{
    ref.func();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
