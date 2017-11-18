/**
 * This file explores the merger of the pImpl idiom with an abstract base class.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PimplABS.cpp
 *  Created on: Oct 30, 2017
 *      Author: Steven R. Emmerson
 */

#include "PimplABS.h"

class Base::Impl
{
public:
    virtual ~Impl()
    {}
    virtual void func() =0;
};

Base::Base(Impl* impl)
    : pImpl{impl}
{}

Base::~Base()
{}

void Base::func()
{
    pImpl->func();
}

/******************************************************************************/

class Derived::Impl : public Base::Impl
{
public:
    ~Impl()
    {}
    void func() override
    {}
};

Derived::Derived()
    : Base{new Derived::Impl()}
{}

/******************************************************************************/

class Client
{
    Base base;
public:
    Client()
        : base{Derived{}}
    {}
};

int main()
{
    Client client{};
}
