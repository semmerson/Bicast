/**
 * This file declares the abstract base class for an ingester of data-products.
 * An ingester provides a sequence of products for a source-node to transmit.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Ingester.h
 *  Created on: Oct 24, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PROD_INGESTER_H_
#define MAIN_PROD_INGESTER_H_

#include "Product.h"

#include <memory>

namespace hycast {

class Ingester
{
protected:
    class Impl
    {
    public:
        virtual ~Impl()
        {}

        virtual Product getProduct() =0;
    };

    Ingester(Impl* impl)
        : pImpl{impl}
    {}

private:
    std::shared_ptr<Impl> pImpl;

public:
    Ingester()
        : pImpl{}
    {}

    virtual ~Ingester() =0;

    inline operator bool()
    {
        return pImpl.operator bool();
    }

    inline Product getProduct()
    {
        return pImpl->getProduct();
    }
};

} // namespace

#endif /* MAIN_PROD_INGESTER_H_ */
