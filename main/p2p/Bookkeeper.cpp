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

#include <p2p/Trigger.h>
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
 * Bookkeeper base class implementation.
 */
class BookkeeperImpl : public Bookkeeper
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
    BookkeeperImpl()
        : mutex()
    {}

    BookkeeperImpl(const BookkeeperImpl& other) =delete;
    BookkeeperImpl& operator=(const BookkeeperImpl& rhs) =delete;

    virtual bool add(const Peer peer) =0;

    virtual bool erase(const Peer peer) =0;

    virtual void requested(const Peer peer) override {
        LOG_ASSERT("Unsupported function");
    }

    virtual bool shouldNotify(
            Peer            peer,
            const ProdIndex prodIndex) const override {
        LOG_ASSERT("Unsupported function");
    }

    virtual bool shouldNotify(
            Peer            peer,
            const DataSegId dataSegId) const override {
        LOG_ASSERT("Unsupported function");
    }

    virtual bool shouldRequest(
            Peer            peer,
            const ProdIndex prodindex) override {
        LOG_ASSERT("Unsupported function");
    }

    virtual bool shouldRequest(
            Peer            peer,
            const DataSegId dataSegId) override {
        LOG_ASSERT("Unsupported function");
    }

    virtual bool received(
            Peer            peer,
            const ProdIndex prodIndex) override {
        LOG_ASSERT("Unsupported function");
    }

    virtual bool received(
            Peer            peer,
            const DataSegId datasegId) override {
        LOG_ASSERT("Unsupported function");
    }

    virtual void erase(const ProdIndex prodIndex) override {
        LOG_ASSERT("Unsupported function");
    }

    virtual void erase(const DataSegId dataSegId) override {
        LOG_ASSERT("Unsupported function");
    }

    virtual Peer getAltPeer(
            const Peer      peer,
            const ProdIndex prodIndex) override {
        LOG_ASSERT("Unsupported function");
    }

    virtual Peer getAltPeer(
            const Peer      peer,
            const DataSegId dataSegId) override {
        LOG_ASSERT("Unsupported function");
    }

    virtual Peer getWorstPeer() const override =0;

    virtual void reset() noexcept override =0;
};

/******************************************************************************/

/**
 * Bookkeeper implementation for a publisher
 */
class PubBookkeeper final : public BookkeeperImpl
{
    using RequestCount = int_fast32_t;

    /// Map of peer -> number of requests by remote peer
    std::unordered_map<Peer, RequestCount> numRequests;

public:
    PubBookkeeper(const int maxPeers)
        : BookkeeperImpl()
        , numRequests(maxPeers)
    {}

    bool add(const Peer peer) override {
        Guard guard(mutex);
        return numRequests.insert({peer, 0}).second;
    }

    void requested(const Peer peer) override {
        Guard guard(mutex);
        ++numRequests[peer];
    }

    Peer getWorstPeer() const override {
        Peer          peer{};
        RequestCount  maxCount = -1;
        Guard         guard(mutex);

        for (auto& elt : numRequests) {
            const auto count = elt.second;

            if (count && count > maxCount) {
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

Bookkeeper::Pimpl Bookkeeper::createPub(const int maxPeers) {
    return Pimpl{new PubBookkeeper(maxPeers)};
}

/******************************************************************************/

/**
 * Bookkeeper implementation for a subscriber
 */
class SubBookkeeper final : public BookkeeperImpl
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
    inline void vetPeer(Peer peer) const {
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
            const DatumId& datumId) const
    {
        Guard guard(mutex);

        vetPeer(peer);

        /*
         * The peer should be notified if no remote peer has announced that it
         * has the given datum or the given peer hasn't announced that it has.
         */
        return datumPeersMap.count(datumId) == 0 ||
                datumPeersMap.at(datumId).set.count(peer) == 0;
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
         * A request for the given datum should be made if there are no peers
         * for the given datum currently.
         */
        bool  should = datumPeersMap.count(datumId) == 0;
        /*
         * In any case, the given peer must now be added to the set of peers
         * that have the datum.
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
            for (auto& peer : datumPeersMap[datumId].list)
                peerInfoMap.at(peer).datumIds.erase(datumId);
            datumPeersMap.erase(datumId);
        }
    }

    /**
     * Returns the best alternative peer for a datum.
     *
     * @param[in] badPeer  Peer that couldn't supply the datum. It will be
     *                     removed from the set of peers that indicated they
     *                     have the datum.
     * @param[in] datumId  Datum ID
     * @return             Best alternative peer. Will test false if none exist.
     */
    Peer getAltPeer(
            const Peer     badPeer,
            const DatumId& datumId) {
        Guard guard{mutex};
        Peer  altPeer;

        const auto count = peerInfoMap.at(badPeer).datumIds.erase(datumId);
        LOG_ASSERT(count == 1);

        auto& peers = datumPeersMap.at(datumId).list;
        if (peers.size()) {
            LOG_ASSERT(peers.front() == badPeer);
            peers.pop_front();
            if (peers.size())
                altPeer = peers.front();
        }

        return altPeer;
    }

public:
    /**
     * Constructs.
     *
     * @param[in] maxPeers        Maximum number of peers
     * @throws std::system_error  Out of memory
     * @cancellationpoint         No
     */
    SubBookkeeper(const int maxPeers)
        : BookkeeperImpl()
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
    bool add(Peer peer) override {
        Guard guard(mutex);
        return peerInfoMap.insert({peer, PeerInfo{}}).second;
    }

    bool shouldNotify(
            Peer            peer,
            const ProdIndex prodIndex) const override {
        return shouldNotify(peer, DatumId(prodIndex));
    }

    bool shouldNotify(
            Peer            peer,
            const DataSegId dataSegId) const override {
        return shouldNotify(peer, DatumId(dataSegId));
    }

    bool shouldRequest(
            Peer            peer,
            const ProdIndex prodIndex) override {
        return shouldRequest(peer, DatumId(prodIndex));
    }

    bool shouldRequest(
            Peer            peer,
            const DataSegId dataSegId) override {
        return shouldRequest(peer, DatumId(dataSegId));
    }

    bool received(Peer            peer,
                  const ProdIndex prodIndex) override {
        return received(peer, DatumId{prodIndex});
    }

    bool received(Peer            peer,
                  const DataSegId dataSegId) override {
        return received(peer, DatumId{dataSegId});
    }

    void erase(const ProdIndex prodIndex) override {
        erase(DatumId(prodIndex));
    }

    void erase(const DataSegId dataSegId) override {
        erase(DatumId(dataSegId));
    }

    Peer getAltPeer(
            const Peer      peer,
            const ProdIndex prodIndex) override {
        return getAltPeer(peer, DatumId{prodIndex});
    }

    Peer getAltPeer(
            const Peer      peer,
            const DataSegId dataSegId) override {
        return getAltPeer(peer, DatumId{dataSegId});
    }

    /**
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    Peer getWorstPeer() const override
    {
        static Peer noPeer{};
        Peer        peer{};
        Rating      minRating = ~(Rating)0;
        bool        valid = false;
        Guard       guard(mutex);

        for (auto& elt : peerInfoMap) {
            if (elt.first.isClient()) {
                const auto rating = elt.second.rating;
                if (rating)
                    valid = true;
                if (rating < minRating) {
                    minRating = rating;
                    peer = elt.first;
                }
            }
        }

        return valid ? peer : noPeer;
    }

    /**
     * Resets the rating of every peer.
     *
     * @threadsafety       Safe
     * @exceptionsafety    No throw
     * @cancellationpoint  No
     */
    void reset() noexcept override {
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
            for (const auto& datumId : peerInfoMap[peer].datumIds) {
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

Bookkeeper::Pimpl Bookkeeper::createSub(const int maxPeers) {
    return Pimpl{new SubBookkeeper(maxPeers)};
}

} // namespace
