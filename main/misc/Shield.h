/**
 * This file declares a RAII class for disabling thread cancellation.
 *
 *        File: Shield.h
 *  Created on: May 27, 2021
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

#ifndef MAIN_MISC_SHIELD_H_
#define MAIN_MISC_SHIELD_H_

#include <pthread.h>

namespace hycast {

/**
 * RAII class that disables thread cancellation on construction and returns it
 * to its previous state on destruction.
 */
class Shield final
{
    int previous;

public:
    /**
     * Sets thread cancellation state.
     */
    Shield()
        : previous()
    {
        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &previous);
    }

    /**
     * Copy constructs.
     * @param[in] shield  The other instance
     */
    Shield(const Shield& shield) =delete;

    /**
     * Returns thread cancellation to previous state.
     */
    ~Shield()
    {
        ::pthread_setcancelstate(previous, &previous);
    }

    /**
     * Copy assisgns.
     * @param[in] rhs  The instance on the right-hand-side
     * @return         Reference to this just-assigned instance
     */
    Shield& operator=(const Shield& rhs) =delete;
};

} // Namespace

#endif /* MAIN_MISC_SHIELD_H_ */
