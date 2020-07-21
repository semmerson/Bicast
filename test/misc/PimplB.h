/**
 * This file tests mutually-dependent pImpl-s.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PimplB.h
 *  Created on: Jul 20, 2020
 *      Author: Steven R. Emmerson
 */

#ifndef TEST_MISC_PIMPLB_H_
#define TEST_MISC_PIMPLB_H_

#include <memory>

namespace hycast {

class PimplA;

class PimplB
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    PimplB(std::shared_ptr<Impl> impl);

public:
    /**
     * Constructs.
     */
    PimplB(PimplA& pimplA);

    void foo();
};

} // namespace

#endif /* TEST_MISC_PIMPLB_H_ */
