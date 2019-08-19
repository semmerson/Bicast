/**
 * A vector for scatter/gather I/O.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: IoVec.cpp
 *  Created on: May 6, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "IoVec.h"

#include <arpa/inet.h>
#include <stdexcept>

namespace hycast {

void IoVec::add(void* data, const size_t nbytes)
{
    if (cnt >= iov_max)
        throw std::out_of_range("Too many scatter/gather I/O elements");

    vec[cnt].iov_base = data;
    vec[cnt++].iov_len = nbytes;
}

void IoVec::add(const uint32_t value)
{
    uint32s[uint32Cnt] = htonl(value);
    add(&uint32s[uint32Cnt], sizeof(uint32s[0]));
    ++uint32Cnt;
}

} // namespace
