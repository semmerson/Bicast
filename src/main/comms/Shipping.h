/**
 * This file declares a component that ships data-products to receiving nodes
 * using both multicast and peer-to-peer transports.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Shipping.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_COMMS_SHIPPING_H_
#define MAIN_COMMS_SHIPPING_H_

#include "Product.h"

#include <memory>

namespace hycast {

class Shipping final
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs.
     */
    Shipping(
            const std::string& pathname,
            const double       residence);

    /**
     * Constructs.
     */
    Shipping(const std::string& pathname);

    /**
     * Constructs.
     */
    Shipping(const double residence);

    /**
     * Ships a product.
     * @param[in] prod  Product to ship
     */
    void ship(Product& prod);
};

} // namespace

#endif /* MAIN_COMMS_SHIPPING_H_ */
