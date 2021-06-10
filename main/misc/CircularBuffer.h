/**
 * This file declares a thread-safe circular buffer.
 *
 *  @file:  CircularBuffer.h
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

#ifndef MAIN_MISC_CIRCULARBUFFER_H_
#define MAIN_MISC_CIRCULARBUFFER_H_

#include <condition_variable>
#include <mutex>

namespace hycast {

template<class ELT>
class CircBuf
{
private:
    // The pImpl idiom isn't used because it doesn't play well with templates

    using Mutex = std::mutex;
    using Guard = std::lock_guard<Mutex>;
    using Lock  = std::unique_lock<Mutex>;
    using Cond  = std::condition_variable;

    Mutex  mutex;
    Cond   cond;
    size_t size;
    size_t read;
    size_t write;
    ELT*   buf;

public:
    /**
     * Default constructs.
     */
    CircBuf() =delete;

    /**
     * Constructs with a capacity.
     *
     * @param[in] capacity           Maximum number of elements. Must be
     *                               positive.
     * @throw std::invalid_argument  Capacity is zero
     */
    CircBuf(size_t capacity);

    CircBuf(const CircBuf& circBuf);

    ~CircBuf();

    CircBuf& operator=(const CircBuf& rhs);

    bool put(ELT& elt);

    ELT get();
};

} // namespace

#endif /* MAIN_MISC_CIRCULARBUFFER_H_ */
