/**
 * This file tests mutually-dependent pImpl-s.
 *
 *        File: PimplA.h
 *  Created on: Jul 20, 2020
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

#ifndef TEST_MISC_PIMPLA_H_
#define TEST_MISC_PIMPLA_H_

#include "PimplB.h"

#include <memory>

namespace hycast {

class PimplA
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    PimplA(std::shared_ptr<Impl> impl);

public:
    /**
     * Default constructs.

     */
    PimplA();
    void foo();

    void bar(PimplB& pimplB);
};

} // namespace

#endif /* TEST_MISC_PIMPLA_H_ */
