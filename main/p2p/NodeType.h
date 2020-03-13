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

#ifndef MAIN_P2P_NODETYPE_H_
#define MAIN_P2P_NODETYPE_H_

#include <atomic>

namespace hycast {

class NodeType
{
public:
    typedef enum {
        IS_SOURCE,        ///< Node is source (=> PATH_TO_SOURCE)
        PATH_TO_SOURCE,   ///< Node isn't source but has path to source
        NO_PATH_TO_SOURCE ///< Node doesn't have path to source
    } Type;

private:
    std::atomic<Type> type;

public:
    NodeType(const int type);

    NodeType()
        : NodeType(NO_PATH_TO_SOURCE)
    {}

    NodeType& operator =(const Type rhs);

    operator unsigned();
};

} // namespace

#endif /* MAIN_P2P_NODETYPE_H_ */
