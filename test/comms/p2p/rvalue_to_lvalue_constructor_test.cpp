/**
 * This file tests whether one constructor can call an
 * lvalue-reference-accepting contructor with an rvalue.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: rvalue_to_lvalue_constructor_test.cpp
 * @author: Steven R. Emmerson
 */
#include <functional>

class Bar final {
public:
    Bar() {};
};

class Foo final {
public:
    Foo(Bar& arg) {}
    Foo(Bar&& arg) : Foo(std::ref<Bar>(arg)) {} // This works
    Foo() : Foo(Bar()) {}
};
