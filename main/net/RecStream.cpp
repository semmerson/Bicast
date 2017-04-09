/**
 * This file implements non-virtual functions of streams that preserve record
 * boundaries.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: RecStream.cpp
 * @author: Steven R. Emmerson
 */

#include "RecStream.h"

namespace hycast {

size_t InRecStream::recv(
        void* const  buf,
        const size_t nbytes,
        const bool   peek)
{
    struct iovec iovec;
    iovec.iov_base = buf;
    iovec.iov_len = nbytes;
    return recv(&iovec, 1, peek);
}

void OutRecStream::send(
        const void* const buf,
        const size_t      nbytes)
{
    struct iovec iovec;
    iovec.iov_base = const_cast<void*>(buf);
    iovec.iov_len = nbytes;
    send(&iovec, 1);
}

} // namespace
