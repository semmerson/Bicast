/**
 * This file declares the API for logging.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: logging.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_LOGGING_H_
#define MAIN_LOGGING_H_

#include <exception>
#include <iostream>

namespace hycast {

void log_what(const std::exception& except);

} // namespace

#endif /* MAIN_LOGGING_H_ */
