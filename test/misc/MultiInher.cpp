#include "MultiInher.h"

class BaseImpl
{
public:
    virtual ~BaseImpl() =default;
};

class Class1 : public BaseImpl, public Iface1
{
public:
    Class1()
        : BaseImpl()
    {}

    virtual ~Class1() =default;

    void func1() override {
    }
};

class Class2 : public Class1, public Iface2
{
public:
    Class2()
        : Class1()
    {}

    ~Class2() =default;

    void func2() override {
    }
};
