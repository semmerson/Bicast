#include "Diamond.h"

class BaseImpl : virtual public BaseIface
{
public:
    virtual ~BaseImpl() =default;
};

class DerivedImpl final : public DerivedIface, public BaseImpl
{
public:
    DerivedImpl()
        : BaseImpl()
    {}

    void func() const override {
    }
};
