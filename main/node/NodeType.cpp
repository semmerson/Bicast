/**
 * The type of node in a peer-to-peer network.
 *
 *        File: NodeType.cpp
 *  Created on: Mar 9, 2020
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
#include "NodeType.h"

namespace hycast {

NodeType::NodeType(const int value)
{
    if (value < 0 || value > 2)
        throw INVALID_ARGUMENT("Node-type " + std::to_string(value) +
                " is invalid");
    this->value = value;
}

const NodeType NodeType::PUBLISHER = NodeType(0);
const NodeType NodeType::PATH_TO_PUBLISHER = NodeType(1);
const NodeType NodeType::NO_PATH_TO_PUBLISHER = NodeType(2);

NodeType::NodeType(const NodeType& nodeType)
{
    value = nodeType.value.load();
}

NodeType::operator unsigned() const noexcept
{
    return value;
}

NodeType& NodeType::operator =(const NodeType& rhs) noexcept
{
    value = rhs.value.load();
    return *this;
}

bool NodeType::operator ==(const NodeType& rhs) const noexcept
{
    return value == rhs.value;
}

} // namespace
