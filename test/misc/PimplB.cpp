/**
 * This file tests mutually-dependent pImpl-s.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PimplB.cpp
 *  Created on: Jul 20, 2020
 *      Author: Steven R. Emmerson
 */

#include "PimplB.h"

namespace hycast {

class PimplB::Impl {
    PimplA pimplA;

public:
    Impl(PimplA& pimplA)
        : pimplA(pimplA)
    {}

    void foo() {
        pimplA.bar(PimplB(pimplA));
    }
};

PimplB::PimplB(PimplA& pimplA)
    : pImpl{std::make_shared<Impl>(pimplA)}
{}

void PimplB::foo() {
    pImpl->foo();
}

} // namespace
