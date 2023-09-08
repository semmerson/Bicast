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

#include <functional>
#include <memory>

namespace hycast {

/// A class for the local disposition (i.e., processing) of data products
class Disposer
{
public:
    class                 Impl;

private:
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Factory-method for creating a Disposer. This method allows the SubNode to create its Disposer
     * independent of how the Disposer is created.
     * @param[in] lastProcDir  Pathname of directory containing information on the last,
     *                         successfully-processed data-product
     * @param[in] feedName     Name of the data-product feed
     */
    using Factory = std::function<Disposer(const String& lastProdDir, const String& feedName)>;

    /**
     * Default constructs. The resulting Disposer will not be valid and will test false.
     * @see operator bool()
     */
    Disposer();

    /**
     * Constructs. The `dispose()` method will do nothing until `add()` is called. This constructor
     * exists to support unit-testing of the Disposer class independent of a configuration-file
     * parser.
     * @param[in] lastProcDir    Pathname of the directory to hold information on the last,
     *                           successfully-processed data-product
     * @param[in] feedName       Name of the data-product feed
     * @param[in] maxPersistent  Maximum number of persistent actions (i.e., actions whose file
     *                           descriptors are kept open)
     * @see `add()`
     */
    Disposer(
            const String& lastProcDir,
            const String  feedName,
            const int     maxPersistent = 20);

    /**
     * Creates a Disposer instance based on a YAML configuration-file.
     *
     * @param[in] configFile     Pathname of the configuration-file
     * @param[in] feedName       Name of the data-product feed
     * @param[in] lastProcDir    Default pathname of the directory to hold information on the last,
     *                           successfully-processed data-product
     * @param[in] maxPersistent  Default maximum number of persistent actions (i.e., actions whose
     *                           file descriptors are kept open)
     * @return                   A Disposer corresponding to the configuration-file
     * @throw InvalidArgument    Couldn't load configuration-file
     * @throw SystemError        Couldn't get pathname of current working directory
     * @throw RuntimeError       Couldn't parse configuration-file
     */
    static Disposer createFromYaml(
            const String& configFile,
            const String& feedName,
            String        lastProcDir,
            int           maxPersistent = 20);

    /**
     * Indicates if this is a valid instance (i.e., not default constructed).
     * @retval true   This is a valid instance
     * @retval false  This is not a valid instance
     */
    operator bool() const noexcept;

    /**
     * Adds a pattern-action.
     *
     * @param[in] patternAction  Pattern-action to be added
     */
    void add(const PatternAction& patternAction) const;

    /**
     * Returns the modification-time of the last, successfully-processed data-product.
     * @return The modification-time of the last, successfully-processed data-product if this
     *         instance is valid and the time exists; otherwise, SysTimePoint::min()
     */
    SysTimePoint getLastProcTime() const;

    /**
     * Returns the number of pattern/action entries.
     * @return The number of pattern/action entries. Will be 0 if `operator bool()` is false.
     * @see operator bool()
     */
    size_t size() const;

    /**
     * Returns the YAML representation.
     * @return The YAML representation
     */
    String getYaml();

    /**
     * Disposes of a product.
     *
     * @param[in] prodInfo  Product metadata
     * @param[in] bytes     Product data. There shall be `prodInfo.getSize()` bytes.
     * @param[in] path      Pathname of the underlying file
     * @retval    true      Disposition was successful
     * @retval    false     Disposition was not successful
     */
    bool dispose(
            const ProdInfo prodInfo,
            const char*    bytes,
            const String&  path) const;
};

} // namespace

#endif /* MAIN_DISPOSER_DISPOSER_H_ */
