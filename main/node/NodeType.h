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

class NodeType : public XprtAble
{
public:
    using Type = uint8_t;

private:
    enum Mask : uint8_t {
        PUBLISHER = 0x1,
        PATH_TO_PUBLISHER = 0x2
    };

    std::atomic<uint8_t>  mask;

public:
    constexpr NodeType(const bool isPublisher)
        : mask{static_cast<Type>(isPublisher ? PUBLISHER | PATH_TO_PUBLISHER : 0)}
    {}

    constexpr NodeType()
        : NodeType{false}
    {}

    inline void setPathToPub(const bool pathToPub) noexcept {
        mask.fetch_or(PATH_TO_PUBLISHER);
    }

    inline bool isPathToPub() const noexcept {
        return mask.load() & PATH_TO_PUBLISHER;
    }

    inline std::string to_string() const {
        return std::to_string(static_cast<Type>(mask));
    }

    inline bool isPublisher() const {
        return mask.load() & PUBLISHER;
    }

    inline bool isSubscriber() const {
        return !isPublisher();
    }

    inline bool hasPathToPub() const {
        return mask.load() & (PUBLISHER | PATH_TO_PUBLISHER);
    }

    inline operator Type() const noexcept {
        return static_cast<Type>(mask);
    }

    inline bool operator==(const NodeType& rhs) const noexcept {
        return mask.load() == rhs.mask.load();
    }

    inline bool operator!=(const NodeType& rhs) const noexcept {
        return !(*this == rhs);
    }

    inline bool write(Xprt xprt) const override {
        return xprt.write(static_cast<Type>(mask));
    }

    bool read(Xprt xprt) override;
};

} // namespace

#endif /* MAIN_NODE_NODETYPE_H_ */
