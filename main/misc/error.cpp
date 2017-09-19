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
        const std::string msg)
    : std::invalid_argument{makeWhat(file, line, msg)}
{}

LogicError::LogicError(
        const char*       file,
        const int         line,
        const std::string msg)
    : std::logic_error{makeWhat(file, line, msg)}
{}

NotFoundError::NotFoundError(
        const char*       file,
        const int         line,
        const std::string msg)
    : std::runtime_error{makeWhat(file, line, msg)}
{}

OutOfRange::OutOfRange(
        const char*       file,
        const int         line,
        const std::string msg)
    : std::out_of_range{makeWhat(file, line, msg)}
{}

RuntimeError::RuntimeError(
        const char*       file,
        const int         line,
        const std::string msg)
    : std::runtime_error{makeWhat(file, line, msg)}
{}

SystemError::SystemError(
        const char*       file,
        const int         line,
        const std::string msg,
        const int         errnum)
    : std::system_error{errnum, std::system_category(),
            makeWhat(file, line, msg)}
{}

} // namespace
