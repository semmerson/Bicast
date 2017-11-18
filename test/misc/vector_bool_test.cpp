/**
 * This file 
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: vector_bool_test.cpp
 *  Created on: Nov 2, 2017
 *      Author: Steven R. Emmerson
 */

#include <iostream>
#include <vector>

int main()
{
    std::vector<bool> vb(0);
    long num = 0;
    if (num+1 > vb.size())
        vb.resize(num+1);
    vb[num] = true;
    std::cout << vb.size() << std::endl;
}
