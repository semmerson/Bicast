#include <memory>

// Header-file:
template<class T> class Base {
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;
    Base(Impl* impl);
public:
    virtual ~Base();
    void concrete();
    virtual void abstract() =0;
};
/******************************************************************************/
class Derived : public Base<int> {
    class Impl;
public:
    Derived(int i);
    void abstract();
};
/******************************************************************************/
/******************************************************************************/
// Source-file:
template<class T> class Base<T>::Impl {
    T t;
public:
    Impl(T t) : t{t} {}
    virtual ~Impl();
    void concrete() {}
    virtual void abstract() =0;
};

template<> Base<int>::Base(Impl* impl) : pImpl{impl} {}

template<> void Base<int>::concrete() {pImpl->concrete();}
/******************************************************************************/
class Derived::Impl : public Base<int>::Impl {
public:
    Impl(int i) : Base::Impl{i} {}
    void abstract() {}
};

Derived::Derived(int i) : Base(new Impl(i)) {}

void Derived::abstract() {static_cast<Derived::Impl*>(pImpl.get())->abstract();}
