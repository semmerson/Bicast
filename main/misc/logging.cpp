/**
 * This file implements logging.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: logging.cpp
 * @author: Steven R. Emmerson
 */

#include "error.h"

#include <iostream>

namespace hycast {

void log_what(const std::exception& except)
{
    try {
        std::rethrow_if_nested(except);
    }
    catch (const std::exception& inner) {
        log_what(inner);
    }
    std::clog << except.what() << '\n';
}

void log_what(
        const std::exception& except,
        const char* const     file,
        const int             line,
        const std::string     msg)
{
    log_what(except);
    std::clog << placeStamp(file, line) << ": " << msg << '\n';
}

} // namespace
