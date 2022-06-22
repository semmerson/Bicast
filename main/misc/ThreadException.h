/**
 * This file handles exceptions thrown on threads.
 *
 *  @file: ThreadException.h
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

#ifndef MAIN_MISC_THREADEXCEPTION_H_
#define MAIN_MISC_THREADEXCEPTION_H_

#include <exception>

namespace hycast {

class ThreadEx
{
    using Mutex = std::mutex;
    using Guard = std::lock_guard<Mutex>;
    using ExPtr = std::exception_ptr;

    mutable Mutex mutex;
    ExPtr         exPtr;
    bool          isSet; // Used because `exception_ptr::swap()` slices exceptions

public:
    /**
     * Default constructs.
     */
    ThreadEx()
        : mutex()
        , exPtr()
        , isSet(false)
    {}

    /**
     * Sets the exception if it isn't already set.
     */
    void set(const std::exception& ex) {
        Guard guard(mutex);
        if (!isSet)
            exPtr = std::make_exception_ptr(ex);
    }

    operator bool() {
        Guard guard(mutex);
        return isSet;
    }

    void throwIfSet() {
        Guard guard(mutex);
        if (isSet) {
            // The original exception is thrown because `exPtr.swap()` slices the exception
            isSet = false;
            std::rethrow_exception(exPtr);
        }
    }
};

} // namespace

#endif /* MAIN_MISC_THREADEXCEPTION_H_ */
