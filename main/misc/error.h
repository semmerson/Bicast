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

/// Invalid argument error
class InvalidArgument : public std::invalid_argument
{
public:
    /**
     * Constructs.
     * @param[in] file  The name of the file
     * @param[in] line  The line number in the file
     * @param[in] func  The name of the function
     * @param[in] msg   The log message
     */
    InvalidArgument(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);

/// Macro for creating an invalid argument error
#define INVALID_ARGUMENT(msg) InvalidArgument(__FILE__, __LINE__, __func__, (msg))
};

/// Logic error
class LogicError : public std::logic_error
{
public:
    /**
     * Constructs.
     * @param[in] file  Pathname of the file
     * @param[in] line  Line number in the file
     * @param[in] func  Function in the file
     * @param[in] msg   Log message
     */
    LogicError(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);

};
/// Macro for creating a logic error
#define LOGIC_ERROR(msg) LogicError(__FILE__, __LINE__, __func__, msg)

/// Something wasn't found error
class NotFoundError : public std::runtime_error
{
public:
    /**
     * Constructs.
     * @param[in] file  Name of file
     * @param[in] line  Line number in file
     * @param[in] func  Name of function in file
     * @param[in] msg   Log message
     */
    NotFoundError(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);

/// Macro for creating a not-found error
#define NOT_FOUND_ERROR(msg) NotFoundError(__FILE__, __LINE__, __func__, msg)
};

/// Domain error
class DomainError : public std::domain_error
{
public:
    /**
     * Constructs.
     * @param[in] file  Name of file
     * @param[in] line  Line number in file
     * @param[in] func  Name of function in file
     * @param[in] msg   Log message
     */
    DomainError(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);

/// Macro for using `DomainError`
#define DOMAIN_ERROR(msg) DomainError(__FILE__, __LINE__, __func__, msg)
};

/// Out-of-range error.
class OutOfRange : public std::out_of_range
{
public:
    /**
     * Constructs.
     * @param[in] file  Name of file
     * @param[in] line  Line number in file
     * @param[in] func  Name of function in file
     * @param[in] msg   Log message
     */
    OutOfRange(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);

/// Macro for creating an out-of-range error
#define OUT_OF_RANGE(msg) OutOfRange(__FILE__, __LINE__, __func__, msg)
};

/// Runtime error
class RuntimeError : public std::runtime_error
{
public:
    /**
     * Constructs.
     * @param[in] file  Name of file
     * @param[in] line  Line number in file
     * @param[in] func  Name of function in file
     * @param[in] msg   Log message
     */
    RuntimeError(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);
};
/// Macro for creating a runtime error
#define RUNTIME_ERROR(msg) RuntimeError(__FILE__, __LINE__, __func__, msg)

/// End-of-file error
class EofError : public RuntimeError
{
public:
    /**
     * Constructs.
     * @param[in] file  Name of file
     * @param[in] line  Line number in file
     * @param[in] func  Name of function in file
     * @param[in] msg   Log message
     */
    EofError(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);
};
/// Macro for creating an end-of-file error
#define EOF_ERROR(msg) EofError(__FILE__, __LINE__, __func__, msg)

/// System error
class SystemError : public std::system_error
{
public:
    /**
     * Constructs.
     * @param[in] file    Name of file
     * @param[in] line    Line number in file
     * @param[in] func    Name of function in file
     * @param[in] msg     Log message
     * @param[in] errnum  Error number
     */
    SystemError(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg,
            const int         errnum = errno);

/// Macro for creating a system error
#define SYSTEM_ERROR(...) SystemError(__FILE__, __LINE__, __func__, __VA_ARGS__)
};

/**
 * Constructs a message for an exception.
 * @param[in] file  Name of the file in which the exception occurred
 * @param[in] line  Line number in the file in which the exception occurred
 * @param[in] func  Name of the function in which the exception occurred
 * @param[in] msg   Log message
 * @return          String containg the above information
 */
std::string makeWhat(
        const char*        file,
        const int          line,
        const char*        func,
        const std::string& msg);

/**
 * Replaces `std::terminate()` with one that ensures logging of the current
 * exception if it exists. Calls `abort()`.
 */
void terminate();

} // namespace

#endif /* MAIN_MISC_ERROR_H_ */
