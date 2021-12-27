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

public:
    /**
     * Constructs.
     *
     * @throws std::system_error  Out of memory
     * @cancellationpoint         No
     */
    Impl()
        : mutex()
    {}

    virtual ~Impl() noexcept =default;

    virtual bool add(const Peer peer) =0;

    virtual void reset() noexcept =0;

    virtual bool erase(const Peer peer) =0;
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
    using RequestCount = int_fast32_t;

    /// Map of peer -> number of requests by remote peer
    std::unordered_map<Peer, RequestCount> numRequests;

public:
    Impl(const int maxPeers)
        : Bookkeeper::Impl()
        , numRequests(maxPeers)
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
        RequestCount  maxCount = -1;
        Guard         guard(mutex);

        for (auto& elt : numRequests) {
            const auto count = elt.second;

            if (count > maxCount) {
                maxCount = count;
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

    bool erase(const Peer peer) override {
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

bool PubBookkeeper::erase(const Peer peer) const {
    return static_cast<Impl*>(pImpl.get())->erase(peer);
}

/******************************************************************************/

/**
 * Bookkeeper implementation for a subscriber
 */
class SubBookkeeper::Impl final : public Bookkeeper::Impl
{
    using Rating     = uint_fast32_t;
    using DatumIdSet = std::set<DatumId>;

    struct PeerInfo {
        Rating     rating;   ///< Peer rating
        DatumIdSet datumIds; ///< Data available from remote peer
        PeerInfo()
            : rating(0)
            , datumIds()
        {}
    };

    /**
     * The following supports
     *   - Removal of associated data when the worst peer is removed;
     *   - Removal of associated peers when a datum is received; and
     *   - A peer with a given datum to be in the datum's list of peers at most
     *     once.
     */
    using PeerInfoMap = std::unordered_map<Peer, PeerInfo>;
    struct DatumPeers {
        std::unordered_set<Peer> set;
        std::list<Peer>          list;
    };
    using DatumPeersMap = std::unordered_map<DatumId, DatumPeers>;

    /// Map of peer -> peer entry
    PeerInfoMap peerInfoMap;
    /**
     * Map of datum ID to peers that have the datum.
     */
    DatumPeersMap datumPeersMap;

    /**
     * Vets a peer.
     *
     * @pre                   `mutex` is locked
     * @param[in] peer        Peer to be vetted
     * @throw     LogicError  Peer is unknown
     */
    inline void vetPeer(Peer peer) {
        if (peerInfoMap.count(peer) == 0)
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

        /*
         * The peer should be notified if no remote peer has announced that it
         * has the given datum or the given peer hasn't announced that it has.
         */
        return datumPeersMap.count(datumId) == 0 ||
                datumPeersMap[datumId].set.count(peer) == 0;
    }

    /**
     * Indicates if a request should be made by a peer. The peer is added to the
     * list of peers that have the datum.
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

        /*
         * A request for the given datum should be made if no remote peer has
         * announced that it has the given datum.
         */
        bool  should = datumPeersMap.count(datumId) == 0;
        /*
         * In any case, the given peer should be included in the peers that have
         * the given datum.
         */
        auto& datumPeers = datumPeersMap[datumId];
        if (datumPeers.set.insert(peer).second)
            datumPeers.list.push_back(peer);

        return should;
    }

    /**
     * Process reception of a datum. The rating of the associated peer is
     * increased.
     *
     * @param[in] peer        Peer that received the datum
     * @param[in] datumId     Datum identifier
     * @retval    `true`      Success
     * @retval    `false`     Datum is unknown or remote peer didn't announce
     *                        that it had the datum
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

        if (datumPeersMap.count(datumId) &&
                datumPeersMap[datumId].set.count(peer)) {
            ++(peerInfoMap.at(peer).rating);
            success = true;
        }

        return success;
    }

    void erase(const DatumId& datumId) {
        Guard  guard(mutex);
        if (datumPeersMap.count(datumId)) {
            for (auto peer : datumPeersMap[datumId].list)
                peerInfoMap.at(peer).datumIds.erase(datumId);
            datumPeersMap.erase(datumId);
        }
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
        , peerInfoMap(maxPeers)
        , datumPeersMap()
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
        return peerInfoMap.insert({peer, PeerInfo{}}).second;
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
        erase(DatumId(prodIndex));
    }

    void erase(const DataSegId dataSegId) {
        erase(DatumId(dataSegId));
    }

    /**
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    Peer getWorstPeer() const
    {
        Peer  peer{};
        Guard guard(mutex);

        if (peerInfoMap.size() > 1) {
            Rating minRating = ~(Rating)0;

            for (auto elt : peerInfoMap) {
                const auto rating = elt.second.rating;

                if (rating < minRating) {
                    minRating = rating;
                    peer = elt.first;
                }
            }
        }

        return peer;
    }

    /**
     * Resets the rating of every peer.
     *
     * @threadsafety       Safe
     * @exceptionsafety    No throw
     * @cancellationpoint  No
     */
    void reset() noexcept override
    {
        Guard guard(mutex);

        for (auto& elt : peerInfoMap)
            elt.second.rating = 0;
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
    bool erase(const Peer peer) override
    {
        bool  existed = false;
        Guard guard{mutex};
        if (peerInfoMap.count(peer)) {
            for (const auto datumId : peerInfoMap[peer].datumIds) {
                auto& datumPeers = datumPeersMap.at(datumId);
                auto& peerList = datumPeers.list;
                for (auto iter = peerList.begin(), end = peerList.end();
                        iter != end; ++iter) {
                    if (*iter == peer) {
                        peerList.erase(iter);
                        break;
                    }
                }
                datumPeers.set.erase(peer);
            }
            peerInfoMap.erase(peer);
            existed = true;
        }
        return existed;
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

Peer SubBookkeeper::getWorstPeer() const {
    return static_cast<Impl*>(pImpl.get())->getWorstPeer();
}

bool SubBookkeeper::erase(const Peer peer) const {
    return static_cast<Impl*>(pImpl.get())->erase(peer);
}

} // namespace
