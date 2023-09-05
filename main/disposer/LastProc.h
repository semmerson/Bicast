/**
 * @file LastProc.h
 * Manages access to the time of the last, successfully-processed product.
 * A locally processed product (either sent by a publisher or received by a subscriber) has  an
 * associated creation-time (i.e., the time that the publisher created it). This time is the
 * modification-time of the corresponding product-file and is promulgated as such to subscribers.
 * This creation-time is used to determine if a product-file needs to be locally processed based on
 * the creation-time of the last, successfully-processed product. Obviously, this latter time must
 * persist between sessions and be available at the start of a new session. That is the job of this
 * component.
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

#ifndef MAIN_DISPOSER_LASTPROC_H_
#define MAIN_DISPOSER_LASTPROC_H_

#include "CommonTypes.h"

#include <memory>

namespace hycast {

class LastProc;
using LastProcPtr = std::shared_ptr<LastProc>; ///< Smart pointer to a LastProc

/// Interface for this component
class LastProc
{
public:
    /**
     * Returns a new instance.
     * @param[in] dirPathname  Pathname of the directory to hold the information
     * @param[in] feedName     Name of the data-product feed
     */
    static LastProcPtr create(
            const String& dirPathname,
            const String& feedName);

    /// Destroys.
    virtual ~LastProc() =default;

    /**
     * Saves the time of the last, successfully-processed product-file.
     * @param[in] pathname  Pathname of the last, successfully-processed product-file
     */
    virtual void save(const String& pathname) =0;

    /**
     * Returns the time of the last, successfuly-processed product-file.
     * @return Time of the last, successfully-processed product-file or SysTimePoint::min() if no
     *         such time exists
     */
    virtual SysTimePoint recall() =0;
};

} // namespace

#endif /* MAIN_DISPOSER_LASTPROC_H_ */
