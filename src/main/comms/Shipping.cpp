/**
 * This file implements a component that ships data-products to receiving nodes
 * using both multicast and peer-to-peer transports.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Shipping.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "ProdStore.h"
#include "Shipping.h"

namespace hycast {

class Shipping::Impl final
{
    ProdStore prodStore;

public:
    /**
     * Constructs.
     * @param[in] path        Pathname of file for persisting the products
     *                        between sessions or the empty string to indicate
     *                        no persistence
     * @param[in] residence   Desired minimum residence-time, in seconds, of
     *                        data-products
     * @throw SystemError     Couldn't open temporary persistence-file
     * @throw InvalidArgument Residence-time is negative
     */
    Impl(    const std::string& pathname,
             const double       residence)
        : prodStore(pathname, residence)
    {}

    /**
     * Constructs.
     * @param[in] path        Pathname of file for persisting the products
     *                        between sessions or the empty string to indicate
     *                        no persistence
     * @throw SystemError     Couldn't open temporary persistence-file
     */
    Impl(const std::string& pathname)
        : prodStore(pathname)
    {}

    /**
     * Constructs.
     * @param[in] residence   Desired minimum residence-time, in seconds, of
     *                        data-products
     * @throw InvalidArgument Residence-time is negative
     */
    Impl(const double residence)
        : prodStore(residence)
    {}

    /**
     * Ships a product.
     * @param[in] prod  Product to ship
     */
    void ship(Product& prod)
    {
        throw LogicError(__FILE__, __LINE__, "Not implemented yet");
    }
};

Shipping::Shipping(
        const std::string& pathname,
        const double       residence)
    : pImpl{new Impl(pathname, residence)}
{}

void Shipping::ship(Product& prod)
{
    pImpl->ship(prod);
}

} // namespace
