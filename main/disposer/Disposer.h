/**
 * This file declares a class for disposition (i.e., local processing) of data-products.
 *
 *  @file:  Disposer.h
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2023 University Corporation for Atmospheric Research
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

/// A class for the local disposition of data products
class Disposer
{
public:
    class                 Impl;

private:
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs. The `dispose()` method will do nothing until `add()` is called.
     * @see `add()`
     */
    Disposer();

    /**
     * Sets the maximum number of file descriptors to keep open between products.
     * @param[in] maxKeepOpen  Maximum number of file descriptors to keep open
     */
    void setMaxKeepOpen(const int maxKeepOpen) noexcept;

    /**
     * Returns the maximum number of file descriptors to keep open between products.
     * @return Maximum number of file descriptors to keep open
     */
    int getMaxKeepOpen() const noexcept;

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
    void dispose(
            const ProdInfo prodInfo,
            const char*    bytes) const;

    /**
     * Creates a Disposer instance from a YAML file. This factory method exists
     * in addition to the constructor in order to unit-test the Disposer class
     * independent of a configuration-file parser.
     *
     * @param[in] configFile   Pathname of the configuration-file
     * @return                 A Disposer corresponding to the configuration-file
     * @throw InvalidArgument  Couldn't load configuration-file
     * @throw SystemError      Couldn't get pathname of current working directory
     * @throw RuntimeError     Couldn't parse configuration-file
     */
    static Disposer createFromYaml(const String& configFile);

    /**
     * Returns the YAML representation.
     * @return The YAML representation
     */
    static String getYaml(const Disposer& disposer);
};

} // namespace

#endif /* MAIN_DISPOSER_DISPOSER_H_ */
