/**
 * This file tests mutually-dependent pImpl-s.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PimplAB.cpp
 *  Created on: Jul 20, 2020
 *      Author: Steven R. Emmerson
 */

#include "PimplA.h"
#include "PimplB.h"

int main(int argc, char** argv)
{
    hycast::PimplA pimplA{};
    hycast::PimplB pimplB{pimplA};
    pimplA.foo();
    return 0;
}
