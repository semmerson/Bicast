/**
 * This file tests assigning the reference returned from a function to an `auto`
 * variable.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: AutoReference.cpp
 * Created On: Feb 4, 2020
 *     Author: Steven R. Emmerson
 */
#include <cassert>

static int value = 1;

int& func()
{
    return value;
}

int main(int argc, char **argv)
{
    auto ret1 = func();
    ret1 = 2;
    assert(value == 1); // NB: `value` not changed

    auto& ret2 = func();
    ret2 = 2;
    assert(value == 2); // NB: `value` changed

    return 0;
}
