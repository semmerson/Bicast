/**
 * This file declares a set of peers.
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

    PeerSet(P2pNode& node);

    /**
     * Adds a peer. If the peer is already in the set, then nothing is done;
     * otherwise, the peer is added and starts receiving from its associated
     * remote peer and becomes ready to notify its remote peer.
     *
     * @param[in] peer     Peer to try adding
     * @param[in] pubPath  Is the local peer a path to the publisher?
     * @retval    `false`  Peer was already in the set. Nothing was done.
     * @retval    `true`   Peer was not in the set. Peer was added and started.
     * @see Peer::start()
     */
    bool insert(Peer peer, const bool pubPath = false) const;

    bool erase(Peer peer) const;

    size_type size() const;

    void notify(const PubPath notice) const;

    void notify(const ProdIndex notice) const;

    void notify(const DataSegId& notice) const;
};

} // namespace

#endif /* MAIN_PROTO_PEERSET_H_ */
