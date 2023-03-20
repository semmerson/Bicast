/**
 * Type of node. One that is
 *     - The source of data-products; or
 *     - Has a path to the source of data-products; or
 *     - Doesn't have a path to the source of data-products.
 *
 *        File: NodeType.h
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

#ifndef MAIN_NODE_NODETYPE_H_
#define MAIN_NODE_NODETYPE_H_

#include "InetAddr.h"
#include "Xprt.h"

#include <atomic>

namespace hycast {

/// Type of node
class NodeType : public XprtAble
{
public:
    using Type = uint8_t; ///< Type of node-type indicator

private:
    enum Mask : uint8_t {
        PUBLISHER = 0x1,
        PATH_TO_PUBLISHER = 0x2
    };

    std::atomic<uint8_t>  mask;

public:
    /**
     * Constructs.
     * @param[in] isPublisher  Whether or not this instance is a publisher
     */
    constexpr NodeType(const bool isPublisher)
        : mask{static_cast<Type>(isPublisher ? PUBLISHER | PATH_TO_PUBLISHER : 0)}
    {}

    constexpr NodeType()
        : NodeType{false}
    {}

    /**
     * Sets whether or not this instance has a path to the publisher via its P2P neighbors.
     * @param[in] pathToPub  Does this instance have a path to the publisher?
     */
    inline void setPathToPub(const bool pathToPub) noexcept {
        mask.fetch_or(PATH_TO_PUBLISHER);
    }

    /**
     * Indicates if this instance is a path to the publisher.
     * @retval true     This instance is a path to the publisher
     * @retval false    This instance is not a path to the publisher
     */
    inline bool isPathToPub() const noexcept {
        return mask.load() & PATH_TO_PUBLISHER;
    }

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    inline std::string to_string() const {
        return std::to_string(static_cast<Type>(mask));
    }

    /**
     * Indicates if this node is the publisher.
     * @retval true     This node is the publisher
     * @retval false    This node is not the publisher
     */
    inline bool isPublisher() const {
        return mask.load() & PUBLISHER;
    }

    /**
     * Indicates if this node is the subscriber.
     * @retval true     This node is the subscriber
     * @retval false    This node is not the subscriber
     */
    inline bool isSubscriber() const {
        return !isPublisher();
    }

    /**
     * Indicates if this node has a path to the publisher.
     * @retval true     This node has a path to the publisher
     * @retval false    This node does not have a path to the publisher
     */
    inline bool hasPathToPub() const {
        return mask.load() & (PUBLISHER | PATH_TO_PUBLISHER);
    }

    /**
     * Returns the underlying type of this instance.
     * @return  The underlying type of this instance
     */
    inline operator Type() const noexcept {
        return static_cast<Type>(mask);
    }

    /**
     * Indicates if this instance is equal to another instance.
     * @param[in] rhs      The other instance
     * @retval    true     This instance is equal to the other
     * @retval    false    This instance is not equal to the other
     */
    inline bool operator==(const NodeType& rhs) const noexcept {
        return mask.load() == rhs.mask.load();
    }

    /**
     * Indicates if this instance is not equal to another instance.
     * @param[in] rhs      The other instance
     * @retval    true     This instance is not equal to the other
     * @retval    false    This instance is equal to the other
     */
    inline bool operator!=(const NodeType& rhs) const noexcept {
        return !(*this == rhs);
    }

    /**
     * Writes itself to a transport.
     * @param[in] xprt  The transport
     * @retval    true     Success
     * @retval    false    Connection lost
     */
    inline bool write(Xprt xprt) const override {
        return xprt.write(static_cast<Type>(mask));
    }

    /**
     * Reads itself from a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool read(Xprt xprt) override;
};

} // namespace

#endif /* MAIN_NODE_NODETYPE_H_ */
