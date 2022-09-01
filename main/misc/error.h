/**
 * This file declares the exception and logging mechanism for the package.
 *
 *   @file: error.h
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

#ifndef MAIN_MISC_ERROR_H_
#define MAIN_MISC_ERROR_H_

#include "logging.h"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <signal.h>
#include <stdexcept>
#include <string>
#include <system_error>

namespace hycast {

class InvalidArgument : public std::invalid_argument
{
public:
    InvalidArgument(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);

#define INVALID_ARGUMENT(msg) InvalidArgument(__FILE__, __LINE__, __func__, (msg))
};

class LogicError : public std::logic_error
{
public:
    LogicError(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);

};
#define LOGIC_ERROR(msg) LogicError(__FILE__, __LINE__, __func__, msg)

class NotFoundError : public std::runtime_error
{
public:
    NotFoundError(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);

#define NOT_FOUND_ERROR(msg) NotFoundError(__FILE__, __LINE__, __func__, msg)
};

class DomainError : public std::domain_error
{
public:
    DomainError(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);

#define DOMAIN_ERROR(msg) DomainError(__FILE__, __LINE__, __func__, msg)
};

class OutOfRange : public std::out_of_range
{
public:
    OutOfRange(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);

#define OUT_OF_RANGE(msg) OutOfRange(__FILE__, __LINE__, __func__, msg)
};

class RuntimeError : public std::runtime_error
{
public:
    RuntimeError(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);
};
#define RUNTIME_ERROR(msg) RuntimeError(__FILE__, __LINE__, __func__, msg)

class EofError : public RuntimeError
{
public:
    EofError(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);
};
#define EOF_ERROR(msg) EofError(__FILE__, __LINE__, __func__, msg)

class SystemError : public std::system_error
{
public:
    SystemError(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg,
            const int         errnum = errno);

#define SYSTEM_ERROR(...) SystemError(__FILE__, __LINE__, __func__, __VA_ARGS__)
};

std::string makeWhat(
        const char*        file,
        const int          line,
        const char*        func,
        const std::string& msg);

/**
 * Replaces `std::terminate()` with one that ensures logging of the current
 * exception if it exists. Calls `::abort()`.
 */
void terminate();

} // namespace

#endif /* MAIN_MISC_ERROR_H_ */
