#include <iostream>
#include <string>

class Iface
{
public:
    virtual ~Iface() = default;
    virtual int func() const =0;
};

static void staticFunc(const Iface& iface)
{
    std::cout << iface.func() << '\n';
}

class Class1 : public Iface
{
public:
    int func() const override {
        return 1;
    }
};

class Class2 : public Iface
{
public:
    int func() const override {
        return 2;
    }
};

struct TaggedUnion
{
    enum Tag {
        CLASS1,
        CLASS2
    } tag;
    union {
        Class1 class1;
        Class2 class2;
    };
    TaggedUnion(Class1 arg)
        : tag(CLASS1)
        , class1(arg)
    {}
    TaggedUnion(Class2 arg)
        : tag(CLASS2)
        , class2(arg)
    {}
    ~TaggedUnion() {
        if (tag == CLASS1) class1.~Class1();
        if (tag == CLASS2) class2.~Class2();
    }
};

int main(int argc, char** argv)
{
    Class1      class1{};
    TaggedUnion taggedUnion(class1);
    staticFunc(taggedUnion.class1);
}
