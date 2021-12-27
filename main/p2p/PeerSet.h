/**
 * This file declares a set of active peers that are notified as a group.
 * Consequently, a peer whose remote counterpart belongs to the publisher should
 * not be added.
 *
 *  @file: PeerSet.h
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
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

#ifndef MAIN_PROTO_PEERSET_H_
#define MAIN_PROTO_PEERSET_H_

#include "Peer.h"

#include <memory>

namespace hycast {

class PeerSet
{
public:
    class                 Impl;

protected:
    std::shared_ptr<Impl> pImpl;

public:
    using size_type = size_t;

    PeerSet();

    /**
     * Adds a started peer.
     *
     * NB: A peer whose remote counterpart belongs to the publisher should not
     * be added.
     *
     * @param[in] peer           Peer to be added
     * @throw LogicError         Peer was previously added
     * @throw LogicError         Remote peer belongs to the publisher
     * @throw std::system_error  Couldn't create new thread
     */
    void insert(Peer peer) const;

    void waitForPeer() const;

    bool erase(Peer peer) const;

    size_type size() const;

    /**
     * Notifies all peers about an available product.
     *
     * @param[in] prodIndex  Index of the product
     * @retval    `true`     Success
     * @retval    `false`    Peer set is empty
     */
    bool notify(const ProdIndex prodIndex) const;

    /**
     * Notifies all peers about an available data-segment.
     *
     * @param[in] segId    ID of the data-segment
     * @retval    `true`   Success
     * @retval    `false`  Peer set is empty
     */
    bool notify(const DataSegId segId) const;
};

} // namespace

#endif /* MAIN_PROTO_PEERSET_H_ */
