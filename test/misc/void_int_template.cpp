#include "void_int_template.h"

class BasicFoo::Impl {
public:
    // Something common to both Foo<T> and Foo<void>
    void bar() {
    }
};

BasicFoo::BasicFoo(Impl* ptr)
    : pImpl{ptr}
{}

void BasicFoo::bar() {
    pImpl->bar();
}

/******************************************************************************/

template<class T> class Foo<T>::Impl : public BasicFoo::Impl {
    T value;
public:
    void set(T value) {
        // ... // Useful stuff
        this->value = value;
    }
    T get() {
        // ... // Useful stuff
        return value;
    }
};

template<class T> Foo<T>::Foo()
    : BasicFoo{new Impl()}
{}

template<class T> void Foo<T>::set(T value) {
    reinterpret_cast<Impl*>(pImpl.get())->set(value);
};

template<class T> T Foo<T>::get() {
    return reinterpret_cast<Impl*>(pImpl.get())->get();
};

/******************************************************************************/

class Foo<void>::Impl : public BasicFoo::Impl {
public:
    void set() {
        // ... // Useful stuff
    }
    void get() {
        // ... // Useful stuff
    }
};

Foo<void>::Foo()
    : BasicFoo{new Impl()}
{}

void Foo<void>::set() {
    reinterpret_cast<Impl*>(pImpl.get())->set();
};

void Foo<void>::get() {
    reinterpret_cast<Impl*>(pImpl.get())->get();
};

/******************************************************************************/

#if 1
#include <iostream>

template class Foo<void>;
template class Foo<int>;

int main() {
    Foo<void> voidFoo{};
    voidFoo.set();
    voidFoo.get();

    Foo<int>  intFoo{};
    intFoo.set(1);
    if (intFoo.get() == 1)
        std::cout << "Works!\n";
}
#endif
