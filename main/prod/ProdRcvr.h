/**
 * This file defines the interface for objects that receive data-products.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdRcvr.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_PROD_PRODRCVR_H_
#define MAIN_PROD_PRODRCVR_H_

#include "Product.h"

namespace hycast {

class ProdRcvr
{
public:
    /**
     * Destroys.
     */
    virtual ~ProdRcvr() =default;

    /**
     * Receives a data-product.
     * @param[in] prod  Data-product
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    virtual void recvProd(Product& prod) =0;
};

} // namespace

#endif /* MAIN_PROD_PRODRCVR_H_ */
