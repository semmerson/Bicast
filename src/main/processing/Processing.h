/**
 * This file declares a processor of incoming data-products.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Processing.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_PROCESSING_PROCESSING_H_
#define MAIN_PROCESSING_PROCESSING_H_

#include <memory>
#include <string>

namespace hycast {

class Processing final
{
    class Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs.
     * @param[in] pathname  Pathname of configuration-file
     */
    Processing(const std::string pathname);
};

} // namespace

#endif /* MAIN_PROCESSING_PROCESSING_H_ */
