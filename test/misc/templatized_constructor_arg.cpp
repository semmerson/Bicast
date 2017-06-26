/**
 * This file tests a constructor of a template class whose argument is similarly
 * templatized.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: templatized_constructor_arg.cpp
 *  Created on: Jun 7, 2017
 *      Author: Steven R. Emmerson
 */

template<class T> struct Foo {
    Foo(T value)
    {}
    T get() {
        return T{};
    }
};

template<> struct Foo<void> {
    void get() {}
};

template<class T> struct Bar {
    Foo<T> foo;
    Bar(Foo<T>& foo)
        : foo{foo}
    {}
};

template<> struct Bar<void> {
    Foo<void> foo;
#if 0
    Bar(Foo<void> fooArg)
        : foo{fooArg} // Error: too many initializers for `foo`
    {}
#else
    Bar(Foo<void> fooArg)
        : foo(fooArg)
    {}
#endif
#if 0
    Bar(Foo<void>& fooArg)
        : foo{fooArg} // Error: too many initializers for `foo`
    {}
#else
    Bar(Foo<void>& fooArg)
        : foo(fooArg)
    {}
#endif
};
