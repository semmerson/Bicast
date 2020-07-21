/**
 * This file tests mutually-dependent pImpl-s.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PimplA.h
 *  Created on: Jul 20, 2020
 *      Author: Steven R. Emmerson
 */

#ifndef TEST_MISC_PIMPLA_H_
#define TEST_MISC_PIMPLA_H_

#include "PimplB.h"

#include <memory>

namespace hycast {

class PimplA
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    PimplA(std::shared_ptr<Impl> impl);

public:
    /**
     * Default constructs.

     */
    PimplA();
    void foo();

    void bar(PimplB& pimplB);
};

} // namespace

#endif /* TEST_MISC_PIMPLA_H_ */
