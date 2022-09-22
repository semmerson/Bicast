/**
 * This file declares a class for disposing of (i.e., locally processing) data-products.
 *
 *  @file:  Disposer.h
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */

#ifndef MAIN_DISPOSER_DISPOSER_H_
#define MAIN_DISPOSER_DISPOSER_H_

#include "PatternAction.h"

#include <memory>

namespace hycast {

class Disposer
{
public:
    class                 Impl;

private:
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs.
     *
     * @param[in] maxPersistent  Maximum number of actions that should persist between products
     */
    Disposer(const int maxPersistent);

    /**
     * Adds a pattern-action.
     *
     * @param[in] patternAction  Pattern-action to be added
     */
    void add(const PatternAction& patternAction);

    /**
     * Disposes of a product.
     *
     * @param[in] prodInfo  Product metadata
     * @param[in] bytes     Product data. There shall be `prodInfo.getSize()` bytes.
     */
    void disposeOf(
            const ProdInfo prodInfo,
            const char*    bytes);

    /**
     * Creates a Disposer instance. This factory method exists in addition to the constructor in
     * order to enable unit-testing of the Disposer class without the complication of a
     * configuration-file parser.
     *
     * @param[in] configFile  Pathname of the configuration-file
     * @return                A Disposer corresponding to the configuration-file
     */
    static Disposer create(const String& configFile);
};

} // namespace

#endif /* MAIN_DISPOSER_DISPOSER_H_ */
