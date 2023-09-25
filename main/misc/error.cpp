/**
 * This file implements the module for handling errors.
 *
 *   @file: error.cpp
 * @author: Steven R. Emmerson
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

#include "error.h"

namespace hycast {

InvalidArgument::InvalidArgument(
        const char*       file,
        const int         line,
        const char*       func,
        const std::string msg)
    : std::invalid_argument{makeWhat(file, line, func, msg)}
{}

LogicError::LogicError(
        const char*       file,
        const int         line,
        const char*       func,
        const std::string msg)
    : std::logic_error{makeWhat(file, line, func, msg)}
{}

NotFoundError::NotFoundError(
        const char*       file,
        const int         line,
        const char*       func,
        const std::string msg)
    : std::runtime_error{makeWhat(file, line, func, msg)}
{}

DomainError::DomainError(
        const char*       file,
        const int         line,
        const char*       func,
        const std::string msg)
    : std::domain_error{makeWhat(file, line, func, msg)}
{}

OutOfRange::OutOfRange(
        const char*       file,
        const int         line,
        const char*       func,
        const std::string msg)
    : std::out_of_range{makeWhat(file, line, func, msg)}
{}

RuntimeError::RuntimeError(
        const char*       file,
        const int         line,
        const char*       func,
        const std::string msg)
    : std::runtime_error{makeWhat(file, line, func, msg)}
{}

EofError::EofError(
        const char*       file,
        const int         line,
        const char*       func,
        const std::string msg)
    : RuntimeError{file, line, func, msg}
{}

SystemError::SystemError(
        const char*       file,
        const int         line,
        const char*       func,
        const std::string msg,
        const int         errnum)
    : std::system_error{errnum, std::generic_category(),
            makeWhat(file, line, func, msg)}
{}

void terminate()
{
    auto exPtr = std::current_exception();

    if (!exPtr) {
        LOG_FATAL("terminate() called without an active exception. "
                "A joinable thread object was likely destroyed.");
    }
    else {
        try {
            std::rethrow_exception(exPtr);
        }
        catch (const std::exception& ex) {
            LOG_FATAL(ex);
        }
        catch (...) {
            LOG_FATAL("terminate() called with a non-standard exception");
        }
    }

    ::abort();
}

} // namespace
