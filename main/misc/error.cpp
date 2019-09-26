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

SystemError::SystemError(
        const char*       file,
        const int         line,
        const char*       func,
        const std::string msg,
        const int         errnum)
    : std::system_error{errnum, std::generic_category(),
            makeWhat(file, line, func, msg)}
{}

} // namespace
