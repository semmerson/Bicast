/**
 * Tracks the status of peers in a thread-safe manner.
 *
 *        File: Bookkeeper.cpp
 *  Created on: Oct 17, 2019
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

#include "Bookkeeper.h"
#include "HycastProto.h"
#include "logging.h"

#include <climits>
#include <list>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace hycast {

/**
 * Implementation interface for status monitoring of peers.
 */
class Bookkeeper::Impl
{
protected:
    mutable Mutex mutex;

    /**
     * Constructs.
     *
     * @throws std::system_error  Out of memory
     * @cancellationpoint         No
     */
    Impl()
        : mutex()
    {}

public:
    virtual ~Impl() noexcept =default;

    virtual bool add(const Peer peer) =0;

    virtual void reset() noexcept =0;

    virtual bool remove(const Peer peer) =0;
};

Bookkeeper::Bookkeeper(Impl* impl)
    : pImpl(impl) {
}

/******************************************************************************/

/**
 * Bookkeeper implementation for a publisher
 */
class PubBookkeeper::Impl final : public Bookkeeper::Impl
{
    /// Map of peer -> number of requests by remote peer
    std::unordered_map<Peer, uint_fast32_t> numRequests;

public:
    Impl(const int maxPeers)
        : Bookkeeper::Impl()
        , numRequests()
    {}

    bool add(const Peer peer) override {
        Guard guard(mutex);
        return numRequests.insert({peer, 0}).second;
    }

    void requested(const Peer peer) {
        Guard guard(mutex);
        ++numRequests[peer];
    }

    Peer getWorstPeer() const {
        Peer          peer{};
        unsigned long minCount{ULONG_MAX};
        Guard         guard(mutex);

        for (auto& elt : numRequests) {
            auto count = elt.second;

            if (count < minCount) {
                minCount = count;
                peer = elt.first;
            }
        }

        return peer;
    }

    void reset() noexcept override {
        Guard guard(mutex);

        for (auto& elt : numRequests)
            elt.second = 0;
    }

    bool remove(const Peer peer) override {
        Guard guard(mutex);
        return numRequests.erase(peer) == 1;
    }
};

PubBookkeeper::PubBookkeeper(const int maxPeers)
    : Bookkeeper(new Impl(maxPeers)) {
}

bool PubBookkeeper::add(const Peer peer) const {
    return static_cast<Impl*>(pImpl.get())->add(peer);
}

void PubBookkeeper::requested(const Peer peer) const {
    static_cast<Impl*>(pImpl.get())->requested(peer);
}

Peer PubBookkeeper::getWorstPeer() const {
    return static_cast<Impl*>(pImpl.get())->getWorstPeer();
}

bool PubBookkeeper::remove(const Peer peer) const {
    return static_cast<Impl*>(pImpl.get())->remove(peer);
}

/******************************************************************************/

/**
 * Bookkeeper implementation for a subscriber
 */
class SubBookkeeper::Impl final : public Bookkeeper::Impl
{
    using Ratings   = std::unordered_map<Peer, uint_fast32_t>;
    using PeerList  = std::list<Peer>;
    using PeerLists = std::unordered_map<DatumId, PeerList>;

    /// Map of peer -> peer rating
    Ratings    ratings;
    /**
     * Map of datum ID to list of peers that have the datum in the order in
     * which their notifications arrived.
     */
    PeerLists havePeers;

    /**
     * Vets a peer.
     *
     * @pre                   `mutex` is locked
     * @param[in] peer        Peer to be vetted
     * @throw     LogicError  Peer is unknown
     */
    inline void vetPeer(Peer peer) {
        if (ratings.count(peer) == 0)
            throw LOGIC_ERROR("Peer " + peer.to_string() + " is unknown");
    }

    /**
     * Indicates if a given peer should be notified about an available datum.
     *
     * @param[in] peer        Peer
     * @param[in] datumId     ID of available datum
     * @return    `true`      Notice should be sent
     * @return    `false`     Notice shouldn't be sent
     * @throws    LogicError  Peer is unknown
     * @threadsafety          Safe
     * @cancellationpoint     No
     */
    bool shouldNotify(
            Peer           peer,
            const DatumId& datumId)
    {
        Guard guard(mutex);

        vetPeer(peer);

        bool should = true;
        if (havePeers.count(datumId) != 0) {
            // At least one remote peer has the datum
            auto& list = havePeers[datumId];
            for (auto iter = list.begin(), end = list.end(); iter != end;
                    ++iter) {
                if (*iter == peer) {
                    // Remote peer has the datum => don't notify
                    should = false;
                    break;
                }
            }
        }

        return should;
    }

    /**
     * Indicates if a request should be made by a peer. The peer is
     * added to the list of peers that have the datum.
     *
     * @param[in] peer        Peer
     * @param[in] datumId     Request identifier
     * @return    `true`      Request should be made
     * @return    `false`     Request shouldn't be made
     * @throws    LogicError  Peer is unknown
     * @threadsafety          Safe
     * @cancellationpoint     No
     */
    bool shouldRequest(
            Peer           peer,
            const DatumId& datumId)
    {
        Guard guard(mutex);
        vetPeer(peer);

        //LOG_DEBUG("Peer %s has datum %s", peer.to_string().data(),
                //datumId.to_string().data());
        auto& peerList = havePeers[datumId];
        peerList.push_back(peer); // Add peer. Might throw.
        return peerList.size() == 1;
    }

    /**
     * Process a satisfied request. The rating of the associated peer is
     * increased.
     *
     * @param[in] peer        Peer
     * @param[in] datumId     Request identifier
     * @retval    `true`      Success
     * @retval    `false`     Request is unknown
     * @throws    LogicError  Peer is unknown
     * @threadsafety          Safe
     * @exceptionsafety       Strong guarantee
     * @cancellationpoint     No
     */
    bool received(Peer           peer,
                  const DatumId& datumId)
    {
        bool   success = false;
        Guard  guard(mutex);

        vetPeer(peer);

        if (havePeers.count(datumId)) {
            //LOG_DEBUG("Datum %s has a peer-list", datumId.to_string().data());
            if (havePeers[datumId].front() == peer) {
                //LOG_DEBUG("First peer in list is %s", peer.to_string().data());
                ++ratings.at(peer);
                success = true;
            }
        }

        return success;
    }

public:
    /**
     * Constructs.
     *
     * @param[in] maxPeers        Maximum number of peers
     * @throws std::system_error  Out of memory
     * @cancellationpoint         No
     */
    Impl(const int maxPeers)
        : Bookkeeper::Impl()
        , ratings(maxPeers)
        , havePeers()
    {}

    /**
     * Adds a peer.
     *
     * @param[in] peer            Peer
     * @retval `true`             Success
     * @retval `false`            Not added because already exists
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    bool add(Peer peer) override
    {
        Guard guard(mutex);
        return ratings.insert({peer, 0}).second;
    }

    bool shouldNotify(Peer peer, const ProdIndex prodIndex) {
        return shouldNotify(peer, DatumId(prodIndex));
    }

    bool shouldNotify(Peer peer, const DataSegId dataSegId) {
        return shouldNotify(peer, DatumId(dataSegId));
    }

    bool shouldRequest(Peer peer, const ProdIndex prodIndex) {
        return shouldRequest(peer, DatumId(prodIndex));
    }

    bool shouldRequest(Peer peer, const DataSegId dataSegId) {
        return shouldRequest(peer, DatumId(dataSegId));
    }

    bool received(Peer            peer,
                  const ProdIndex prodIndex) {
        return received(peer, DatumId{prodIndex});
    }

    bool received(Peer            peer,
                  const DataSegId dataSegId) {
        return received(peer, DatumId{dataSegId});
    }

    void erase(const ProdIndex prodIndex) {
        havePeers.erase(DatumId(prodIndex)); // No longer relevant
    }

    void erase(const DataSegId dataSegId) {
        havePeers.erase(DatumId(dataSegId)); // No longer relevant
    }

    /**
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    Peer getWorstPeer(const bool isClient) const
    {
        Peer  peer{};
        Guard guard(mutex);

        if (ratings.size() > 1) {
            unsigned long minCount{ULONG_MAX};

            for (auto elt : ratings) {
                if (elt.first.isClient() == isClient) {
                    const auto count = elt.second;

                    if (count < minCount) {
                        minCount = count;
                        peer = elt.first;
                    }
                }
            }
        }

        return peer;
    }

    /**
     * Resets the count of satisfied requests for every peer.
     *
     * @threadsafety       Safe
     * @exceptionsafety    No throw
     * @cancellationpoint  No
     */
    void reset() noexcept override
    {
        Guard guard(mutex);

        for (auto& elt : ratings)
            elt.second = 0;
    }

    /**
     * Removes a peer.
     *
     * @param[in] peer        The peer to be removed
     * @retval    `true`      Success
     * @retval    `false`     Peer is unknown
     * @threadsafety          Safe
     * @exceptionsafety       Basic guarantee
     * @cancellationpoint     No
     */
    bool remove(const Peer peer) override
    {
        Guard guard{mutex};
        return ratings.erase(peer) == 1;
    }
};

SubBookkeeper::SubBookkeeper(const int maxPeers)
    : Bookkeeper(new Impl(maxPeers)) {
}

bool SubBookkeeper::add(const Peer peer) const {
    return static_cast<Impl*>(pImpl.get())->add(peer);
}

bool SubBookkeeper::shouldNotify(
        Peer            peer,
        const ProdIndex prodIndex) const {
    return static_cast<Impl*>(pImpl.get())->shouldNotify(peer, prodIndex);
}

bool SubBookkeeper::shouldNotify(
        Peer            peer,
        const DataSegId dataSegId) const {
    return static_cast<Impl*>(pImpl.get())->shouldNotify(peer, dataSegId);
}

bool SubBookkeeper::shouldRequest(
        Peer            peer,
        const ProdIndex prodIndex) const {
    return static_cast<Impl*>(pImpl.get())->shouldRequest(peer, prodIndex);
}

bool SubBookkeeper::shouldRequest(
        Peer            peer,
        const DataSegId dataSegId) const {
    return static_cast<Impl*>(pImpl.get())->shouldRequest(peer, dataSegId);
}

bool SubBookkeeper::received(
        Peer            peer,
        const ProdIndex prodIndex) const {
    return static_cast<Impl*>(pImpl.get())->received(peer, prodIndex);
}

bool SubBookkeeper::received(
        Peer            peer,
        const DataSegId dataSegId) const {
    return static_cast<Impl*>(pImpl.get())->received(peer, dataSegId);
}

void SubBookkeeper::erase(const ProdIndex prodIndex) const {
    static_cast<Impl*>(pImpl.get())->erase(prodIndex);
}

void SubBookkeeper::erase(const DataSegId dataSegId) const {
    static_cast<Impl*>(pImpl.get())->erase(dataSegId);
}

Peer SubBookkeeper::getWorstPeer(const bool isClient) const {
    return static_cast<Impl*>(pImpl.get())->getWorstPeer(isClient);
}

bool SubBookkeeper::remove(const Peer peer) const {
    return static_cast<Impl*>(pImpl.get())->remove(peer);
}

} // namespace
