/**
 * A vector for scatter/gather I/O.
 *
 *        File: IoVec.h
 *  Created on: May 6, 2019
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MAIN_INET_IOVEC_H_
#define MAIN_INET_IOVEC_H_

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

#endif /* MAIN_INET_IOVEC_H_ */
