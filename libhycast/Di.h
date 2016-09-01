/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Di.h
 * @author: Steven R. Emmerson
 *
 * This file ...
 */

#ifndef DI_H_
#define DI_H_

#include "ABC.h"

#include <iostream>

class Di : public ABC {
    int& i{*(int*)&storage};
public:
    Di(int j) : i(j) {}
    int get() const {return i;}
    Di& operator=(Di& that) {
        std::cout << "this.i=" << i << '\n';
        std::cout << "that.i=" << that.i << '\n';
        i = that.i;
        std::cout << "this.i=" << i << '\n';
        return *this;
    }
};

#endif /* DI_H_ */
