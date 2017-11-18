/**
 * This file 
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PimplBaseClient.cpp
 *  Created on: Oct 30, 2017
 *      Author: Steven R. Emmerson
 */

#include "PimplABS.h"

class Client
{
    Base base;
public:
    Client()
        : base{Derived{}}
    {}
};
