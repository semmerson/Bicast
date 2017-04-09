/**
 * This file implements a processor of incoming data-products.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Processing.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "Processing.h"

#include <yaml-cpp/yaml.h>

namespace hycast {

class Processing::Impl final
{
    /**
     * Constructs.
     * @param[in] pathname  Pathname of configuration-file
     */
    Impl(const std::string pathname)
    {
        YAML::Node config = YAML::LoadFile(pathname);
    }
};

Processing::Processing(const std::string pathname)
    : pImpl{new Impl(pathname)}
{}

} // namespace
