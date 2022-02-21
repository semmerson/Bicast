#include "MultiInher.h"

class BaseImpl : public BaseIface
{
public:
    void foo() {
    }
};

class DerivedImpl : public FactoryIface, public BaseImpl
{
public:
    using BaseImpl::foo; // Doesn't work

    DerivedImpl()
        : BaseImpl()
    {}

    void foo() { // Works
    }
};

void bar() {
    DerivedImpl();
}
