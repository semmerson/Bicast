/**
 * Interface for objects that can be serialized and deserialized.
 *
 *        File: Serializable.h
 *  Created on: May 3, 2019
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

#ifndef MAIN_INET_SERIALIZABLE_H_
#define MAIN_INET_SERIALIZABLE_H_

#include "Rpc.h"

namespace hycast {

class Serializable
{
public:
    virtual ~Serializable() noexcept
    {}
};

} // namespace

#endif /* MAIN_INET_SERIALIZABLE_H_ */
