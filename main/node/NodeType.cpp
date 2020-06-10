/**
 * 
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: NodeType.cpp
 *  Created on: Mar 9, 2020
 *      Author: Steven R. Emmerson
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
