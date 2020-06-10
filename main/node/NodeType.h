/**
 * Type of node. One that is
 *     - The source of data-products; or
 *     - Has a path to the source of data-products; or
 *     - Doesn't have a path to the source of data-products.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: NodeType.h
 *  Created on: Mar 9, 2020
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_NODE_NODETYPE_H_
#define MAIN_NODE_NODETYPE_H_

#include <atomic>

namespace hycast {

class NodeType
{
private:
    std::atomic_int value;

public:
    static const NodeType PUBLISHER;
    static const NodeType PATH_TO_PUBLISHER;
    static const NodeType NO_PATH_TO_PUBLISHER;

    NodeType(const int value);

    NodeType(const NodeType& type);

    NodeType()
        : NodeType(NO_PATH_TO_PUBLISHER)
    {}

    operator unsigned() const noexcept;

    NodeType& operator =(const NodeType& rhs) noexcept;

    bool operator ==(const NodeType& rhs) const noexcept;
};

} // namespace

#endif /* MAIN_NODE_NODETYPE_H_ */
