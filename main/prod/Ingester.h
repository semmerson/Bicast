/**
 * This file declares an ingester of data-products. One that returns to a source
 * of hycast products a sequence of products to be transmitted.
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

private:
    std::shared_ptr<Impl> pImpl;

protected:
    Ingester(Impl* impl)
        : pImpl{impl}
    {}

public:
    Ingester()
        : pImpl{}
    {}

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
