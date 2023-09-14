/**
 * @file LastProd.h
 * Manages access to the time of the last product transmitted, received, or locally processed. A
 * product has  an associated creation-time (i.e., the time that the publisher created it). This
 * time is the modification-time of the corresponding product-file and is promulgated as such to
 * subscribers. This creation-time is used to determine if a product needs to be transmitted, has
 * been received, or has been locally processed. Obviously, this time must persist between sessions
 * and be available at the start of a new session. That is the job of this component.
 *
 *  Created on: Aug 25, 2023
 *      Author: Steven R. Emmerson
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

#ifndef MAIN_NODE_LASTPROD_H_
#define MAIN_NODE_LASTPROD_H_

#include "CommonTypes.h"

#include <memory>

namespace hycast {

class LastProd;
using LastProdPtr = std::shared_ptr<LastProd>; ///< Smart pointer to an instance

/// Interface for this component
class LastProd
{
public:
    /**
     * Returns a dummy instance for unit-testing. save() will always succeed and recall() will
     * always return SysTimePoint::min().
     */
    static LastProdPtr create();

    /**
     * Returns a new instance.
     * @param[in] pathTemplate  Template for pathname of files to hold information
     * @throw SystemError       Couldn't create a necessary directory
     */
    static LastProdPtr create(const String& pathTemplate);

    /// Destroys.
    virtual ~LastProd() =default;

    /**
     * Saves a time.
     * @param[in] time  The time to be saved
     */
    virtual void save(const SysTimePoint time) =0;

    /**
     * Returns the time of the last, successfuly-processed product-file.
     * @return Time of the last, successfully-processed product-file or SysTimePoint::min() if no
     *         such time exists
     */
    virtual SysTimePoint recall() =0;
};

} // namespace

#endif /* MAIN_NODE_LASTPROD_H_ */
