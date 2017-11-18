/**
 * This file explores the merger of the pImpl idiom with an abstract base class.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PimplABS.h
 *  Created on: Oct 30, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef TEST_PROD_PIMPLABS_H_
#define TEST_PROD_PIMPLABS_H_

#include <memory>

class Base
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;
    /*
     * Defining the following constructor here results in
     * "invalid application of 'sizeof' to incomplete type 'Base::Impl'"
     */
    Base(Impl* impl);
public:
    Base(const Base& base)
        : pImpl{base.pImpl}
    {}
    virtual ~Base();
    void func();
};

class Derived : public Base
{
    class Impl;
public:
    Derived();
};

#endif /* TEST_PROD_PIMPLABS_H_ */
