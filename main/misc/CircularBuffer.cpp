/**
 * This file implements a thread-safe circular buffer.
 *
 *  @file:  CircularBuffer.cpp
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
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

#include "CircularBuffer.h"
#include "error.h"

namespace hycast {

template<class ELT>
CircBuf<ELT>::CircBuf(const size_t capacity)
    : mutex()
    , cond()
    , size(capacity+1)
    , read(0)
    , write(0)
    , buf(new ELT[size])
{
    if (capacity == 0)
        throw INVALID_ARGUMENT("Capacity must be positive");
}

template<class ELT>
CircBuf<ELT>::CircBuf(const CircBuf<ELT>& circBuf)
    : CircBuf(circBuf.size-1)
{}

template<class ELT>
CircBuf<ELT>::~CircBuf() {
    delete[] buf;
}

template<class ELT>
CircBuf<ELT>& operator=(const CircBuf<ELT>& rhs)
{
    Guard guard(mutex);
    delete[] buf;
    buf = new ELT[rhs.size];

}

template<class ELT>
bool CircBuf<ELT>::put(ELT& elt) {
    Guard guard(mutex);

    if ((read + size - write)%size == 1)
        return false;

    buf[write++] = elt;
    write %= size;
    cond.notify_all();

    return true;
}

template<class ELT>
ELT CircBuf<ELT>::get() {
    Lock lock(mutex);

    while (read == write)
        cond.wait(lock);

    const auto curr = read++;
    read %= size;
    return buf[curr];
}

} // namespace
