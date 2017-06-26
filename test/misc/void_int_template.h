#include <memory>

class BasicFoo {
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    BasicFoo(Impl* ptr);

public:
    // Something common to both Foo<T> and Foo<void>
    void bar();
};

template<class T> class Foo : public BasicFoo {
    class Impl;
public:
    Foo();
    void set(T value);
    T get();
};

template<> class Foo<void> : public BasicFoo {
    class Impl;
public:
    Foo();
    void set();
    void get();
};
