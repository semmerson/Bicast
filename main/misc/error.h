/**
 * This file declares the exception and logging mechanism for the package.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: error.h
 * @author: Steven R. Emmerson
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

#define INVALID_ARGUMENT(msg) InvalidArgument(__FILE__, __LINE__, __func__, \
        (msg))
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

#define SYSTEM_ERROR(msg, ...) SystemError(__FILE__, __LINE__, __func__, msg, \
    ##__VA_ARGS__)
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
