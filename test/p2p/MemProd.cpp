/**
 * A data-product that resides in memory.
 *
 *        File: MemProd.cpp
 *  Created on: Oct 13, 2021
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"

#include "error.h"
#include "HycastProto.h"
#include "Socket.h"

#include <string>

namespace hycast {

class MemProd : public Product
{
    ProdInfo    prodInfo;
    const char* data;     ///< Product's data

public:
    /**
     * Default constructs.
     */
    MemProd() =default;

    /**
     * Constructs.
     *
     * @param[in] prodInfo       Product information
     * @param[in] data           Product data. Must exist for the duration of
     *                           this instance. Will not be deleted.
     */
    MemProd(ProdInfo prodInfo, const char* data)
        : prodInfo(prodInfo)
        , data{data}
    {}

    /**
     * Returns the name of this product.
     *
     * @return            Name of this product
     * @throw LogicError  Name has not been set (product information segment
     *                    hasn't been accepted)
     */
    const String& getName() const
    {
        if (!prodInfo)
            throw LOGIC_ERROR("No product information");

        return prodInfo.getName();
    }
};

} // namespace
