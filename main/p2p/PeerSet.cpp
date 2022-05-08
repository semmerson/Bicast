/**
 * This file implements a set of active peers whose remote counterparts can all
 * be notified together.
 *
 *  @file:  PeerSet.cpp
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

#include "config.h"

#include "HycastProto.h"
#include "logging.h"
#include "NoticeArray.h"
#include "PeerSet.h"
#include "ThreadException.h"

#include <map>
#include <pthread.h>
#include <unordered_map>
#include <utility>

namespace hycast {

/**
 * Thread-safe set of active peers.
 */
class PeerSet::Impl
{

    using Peers = std::set<Peer>;

    mutable Mutex mutex; ///< For accessing peers
    mutable Cond  cond;  ///< For accessing peers
    Peers         peers; ///< Set of peers

public:
    Impl()
        : mutex()
        , cond()
        , peers()
    {}

    /**
     * Adds a started peer.
     *
     * @param[in] peer           Peer to be added
     * @throw LogicError         Peer was previously added
     * @throw LogicError         Remote peer belongs to the publisher
     * @throw std::system_error  Couldn't create new thread
     */
    void insert(Peer peer) {
        Guard guard(mutex);

        if (!peers.insert(peer).second)
            throw LOGIC_ERROR("Peer " + peer.to_string() + " was previously "
                    "added");

        cond.notify_all();
    }

    void waitForPeer() {
        Lock lock{mutex};
        cond.wait(lock, [&]{return !peers.empty();});
    }

    size_t size() {
        Guard guard{mutex};
        return peers.size();
    }

    bool erase(Peer peer) {
        Guard guard(mutex);
        return peers.erase(peer);
    }

    Peers::size_type size() const {
        Guard guard(mutex);
        return peers.size();
    }

    bool notify(const ProdId notice) {
        Guard guard{mutex};
        for (auto& peer : peers)
            peer.notify(notice);
        return peers.size();
    }

    bool notify(const DataSegId notice) {
        Guard guard{mutex};
        for (auto& peer : peers)
            peer.notify(notice);
        return peers.size();
    }
};

/******************************************************************************/

PeerSet::PeerSet()
    : pImpl{new Impl()}
{}

void PeerSet::insert(Peer peer) const {
    pImpl->insert(peer);
}

void PeerSet::waitForPeer() const {
    pImpl->waitForPeer();
}

bool PeerSet::erase(Peer peer) const {
    return pImpl->erase(peer);
}

PeerSet::size_type PeerSet::size() const {
    return pImpl->size();
}

bool PeerSet::notify(const ProdId notice) const {
    return pImpl->notify(notice);
}

bool PeerSet::notify(const DataSegId notice) const {
    return pImpl->notify(notice);
}

} // namespace
