/**
 * This file implements the module for handling errors.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: error.cpp
 * @author: Steven R. Emmerson
 */

#include "error.h"

namespace hycast {

std::string placeStamp(
        const char*       file,
        const int         line)
{
    char name[::strlen(file)+1];
    ::strcpy(name, file);
    return std::string(::basename(name)) + ":" + std::to_string(line);
}

InvalidArgument::InvalidArgument(
        const char*       file,
        const int         line,
        const std::string msg)
    : std::invalid_argument{placeStamp(file, line) + ": " + msg}
{}

LogicError::LogicError(
        const char*       file,
        const int         line,
        const std::string msg)
    : std::logic_error{placeStamp(file, line) + ": " + msg}
{}

NotFoundError::NotFoundError(
        const char*       file,
        const int         line,
        const std::string msg)
    : std::runtime_error{placeStamp(file, line) + ": " + msg}
{}

OutOfRange::OutOfRange(
        const char*       file,
        const int         line,
        const std::string msg)
    : std::out_of_range{placeStamp(file, line) + ": " + msg}
{}

RuntimeError::RuntimeError(
        const char*       file,
        const int         line,
        const std::string msg)
    : std::runtime_error{placeStamp(file, line) + ": " + msg}
{}

SystemError::SystemError(
        const char*       file,
        const int         line,
        const std::string msg,
		const int         errnum)
    : std::system_error{errnum, std::system_category(),
            placeStamp(file, line) + ": " + msg}
{}

#if 0
NestedError::NestedError(
        const char*       file,
        const int         line,
        const std::string msg)
    : std::nested_exception{}
	, RuntimeError{file, line, msg}
{
}
#endif

} // namespace
