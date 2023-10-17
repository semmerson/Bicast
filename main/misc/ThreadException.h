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

namespace bicast {

/// The first exception thrown by a thread in a component with multiple threads
class ThreadEx
{
    using Mutex = std::mutex;
    using Guard = std::lock_guard<Mutex>;
    using ExPtr = std::exception_ptr;

    mutable Mutex mutex; ///< For thread safety
    ExPtr         exPtr; ///< Pointer to exception

public:
    /**
     * Default constructs.
     */
    ThreadEx()
        : mutex()
        , exPtr()
    {}

#if 0
    /**
     * Sets the exception if it isn't already set.
     */
    void set(const std::exception& ex) {
        Guard guard{mutex};
        if (!exPtr) {
            LOG_DEBUG("Setting exception pointer to \"%s\"", ex.what());
            exPtr = std::make_exception_ptr(ex);
        }
    }
#else
    /// Sets the exception to the current exception if it's not set.
    void set() {
        Guard guard{mutex};
        if (!exPtr)
            exPtr = std::current_exception();
    }

    /**
     * Sets the exception based on an exception pointer if it's not already set.
     * @param[in] exPtr  Exception pointer
     */
    void set(ExPtr& exPtr) {
        Guard guard{mutex};
        if (!this->exPtr)
            this->exPtr = exPtr;
    }

    /**
     * Sets the exception based on an exception pointer if it's not already set.
     * @param[in] exPtr  Exception pointer
     */
    void set(ExPtr&& exPtr) {
        Guard guard{mutex};
        if (!this->exPtr)
            this->exPtr = exPtr;
    }
#endif

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     * @retval true    This instance is valid
     * @retval false   This instance is not valid
     */
    operator bool() const noexcept {
        Guard guard{mutex};
        return static_cast<bool>(exPtr);
    }

    /**
     * Throws the exception if it exists. Clears the exception.
     */
    void throwIfSet() {
        Guard guard{mutex};
        if (exPtr) {
            ExPtr tmp{};
            tmp.swap(exPtr);
            std::rethrow_exception(tmp);
        }
    }
};

} // namespace

#endif /* MAIN_MISC_THREADEXCEPTION_H_ */
