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

#include "logging.h"

namespace hycast {

void log_what(const std::exception& except)
{
    try {
        std::rethrow_if_nested(except);
    }
    catch (const std::exception& inner) {
        log_what(inner);
    }
    std::cerr << except.what() << '\n';
}

} // namespace
