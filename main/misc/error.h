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

#include <cerrno>
#include <cstring>
#include <exception>
#include <libgen.h>
#include <stdexcept>
#include <string>
#include <system_error>

namespace hycast {

#if 0
class CodeLoc final
{
    const std::string name;
    const int         line;

    std::string makeName(const char* const file)
    {
        char name[::strlen(file)+1];
        ::strcpy(name, file);
        return std::string(::basename(name));
    }

public:
    CodeLoc(
            const char* const file,
            const int         line)
        : name{makeName(file)}
        , line{line}
    {}
};
#endif

std::string placeStamp(
        const char*       file,
        const int         line);

class InvalidArgument : public std::invalid_argument
{
public:
    InvalidArgument(
            const char*       file,
            const int         line,
            const std::string msg);
};

class LogicError : public std::logic_error
{
public:
    LogicError(
            const char*       file,
            const int         line,
            const std::string msg);
};

class NotFoundError : public std::runtime_error
{
public:
    NotFoundError(
            const char*       file,
            const int         line,
            const std::string msg);
};

class OutOfRange : public std::out_of_range
{
public:
    OutOfRange(
            const char*       file,
            const int         line,
            const std::string msg);
};

class RuntimeError : public std::runtime_error
{
public:
    RuntimeError(
            const char*       file,
            const int         line,
            const std::string msg);
};

class SystemError : public std::system_error
{
public:
    SystemError(
            const char*       file,
            const int         line,
            const std::string msg);
};

void log_what(const std::exception& except);
void log_what(
        const std::exception& except,
        const char* const     file,
        const int             line,
        const std::string     msg);

} // namespace

#endif /* MAIN_MISC_ERROR_H_ */
