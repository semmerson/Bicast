/**
 * A vector for scatter/gather I/O.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: IoVec.h
 *  Created on: May 6, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_NET_IO_IOVEC_H_
#define MAIN_NET_IO_IOVEC_H_

#include <stdint.h>
#include <sys/uio.h>

#ifndef _XOPEN_IOV_MAX
#define _XOPEN_IOV_MAX 16
#endif

namespace hycast {

class IoVec final
{
    static const int iov_max = _XOPEN_IOV_MAX;
    uint32_t         uint32s[iov_max];
    int              uint32Cnt;

public:
    struct iovec vec[iov_max];
    int          cnt;

    IoVec()
        : cnt{0}
        , uint32Cnt{0}
    {}

    void add(
            void*        data,
            const size_t nbytes);

    void add(const uint32_t value);
};

} // namespace

#endif /* MAIN_NET_IO_IOVEC_H_ */
