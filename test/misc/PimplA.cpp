/**
 * This file tests mutually-dependent pImpl-s.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PimplA.cpp
 *  Created on: Jul 20, 2020
 *      Author: Steven R. Emmerson
 */

#include "PimplA.h"

namespace hycast {

class PimplA::Impl {
    PimplB pimplB;

public:
    Impl(PimplA& pimplA)
        : pimplB(pimplA)
    {}

    void foo() {
        pimplB.foo();
    }
};

PimplA::PimplA()
    : pImpl{std::make_shared<Impl>(*this)} {
}

void PimplA::foo() {
    pImpl->foo();
}

void PimplA::bar(PimplB& pimplB) {
}

} // namespace
