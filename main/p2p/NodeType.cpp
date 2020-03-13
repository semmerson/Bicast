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

NodeType::NodeType(const int type)
{
    if (type < 0 || type > 2)
        throw INVALID_ARGUMENT("Node-type " + std::to_string(type) +
                " is invalid");
    this->type = static_cast<Type>(type);
}

NodeType& NodeType::operator =(const Type rhs)
{
    type = rhs;
    return *this;
}

NodeType::operator unsigned()
{
    return static_cast<unsigned>(type);
}

} // namespace
