/**
 * This file declares the interface for local processing of received
 * data-products.
 *
 * Copyright 2017 University Corporation for Atmospheric Research.
 * See file "COPYING" in the top-level source-directory for terms and
 * conditions.
 *
 * Processing.h
 *
 *  Created on: Apr 15, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_COMMS_PROCESSING_H_
#define MAIN_COMMS_PROCESSING_H_

#include "Product.h"

namespace hycast {

class Processing
{
public:
    /**
     * Destroys.
     */
    virtual ~Processing()
    {}

    /**
     * Processes a data-product. Returns quickly.
     * @param[in] prod   Data-product to be processed
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     * @throws           RuntimeError
     */
    virtual void process(Product prod) =0;
};

} // namespace

#endif /* MAIN_COMMS_PROCESSING_H_ */
