/**
 * A vector for scatter/gather I/O.
 *
 *        File: IoVec.cpp
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

#include "config.h"

#include <arpa/inet.h>
#include <inet/IoVec.h>
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
