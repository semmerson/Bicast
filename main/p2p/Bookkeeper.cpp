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
#include "Trigger.h"

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
    mutable Mutex mutex; ///< Mutex to protect consistency

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

    /**
     * Copy constructs.
     * @param[in] other  The other instance
     */
    BookkeeperImpl(const BookkeeperImpl& other) =delete;

    virtual ~BookkeeperImpl() {}

    /**
     * Copy assigns.
     * @param[in] rhs  The other, right-hand-side instance
     * @return         A reference to this just-assigned instance
     */
    BookkeeperImpl& operator=(const BookkeeperImpl& rhs) =delete;
};

/******************************************************************************/

/**
 * Bookkeeper implementation for a publisher
 */
class PubBookkeeper final : public BookkeeperImpl
{
    using RequestCount = int_fast32_t;

    /// Map of peer -> number of requests by remote peer
    std::unordered_map<PeerPtr, RequestCount> numRequests;

public:
    /**
     * Constructs.
     * @param[in] maxPeers  Maximum number of peers
     */
    PubBookkeeper(const int maxPeers)
        : BookkeeperImpl()
        , numRequests(maxPeers)
    {}

    bool add(const PeerPtr peer) override {
        Guard guard(mutex);
        return numRequests.insert({peer, 0}).second;
    }

    /**
     * Removes a peer.
     * @param[in] peer  The peer to be removed
     * @retval    true     The peer existed
     * @retval    false    The peer didn't exist
     */
    bool erase(const PeerPtr peer) override {
        Guard guard(mutex);
        return numRequests.erase(peer) == 1;
    }

    /**
     * Handles a peer requesting something.
     * @param[in] peer  The peer that requested something
     */
    void requested(const PeerPtr peer) override {
        Guard guard(mutex);
        ++numRequests[peer];
    }

    bool shouldNotify(
            PeerPtr      peer,
            const ProdId prodId) const override {
        return false;
    }

    bool shouldNotify(
            PeerPtr         peer,
            const DataSegId dataSegId) const override {
        return false;
    }

    bool shouldRequest(
            PeerPtr      peer,
            const ProdId prodindex) override {
        return false;
    }

    bool shouldRequest(
            PeerPtr         peer,
            const DataSegId dataSegId) override {
        return false;
    }

    bool received(
            PeerPtr      peer,
            const ProdId prodId) override {
        return false;
    }

    /**
     * Handles a peer receiving a data segment from its remote counterpart.
     * @param[in] peer     The peer that received the data segment
     * @param[in] segId    The data segment identifier
     * @retval    true     Success
     * @retval    false    The data segment wasn't requested
     */
    bool received(
            PeerPtr         peer,
            const DataSegId segId) override {
        return false;
    }

    /**
     * Deletes all knowledge of a data product.
     * @param[in] prodId  The data product ID
     */
    void erase(const ProdId prodId) override {
    }

    /**
     * Deletes all knowledge of a data segment.
     * @param[in] segId  The data segment ID
     */
    void erase(const DataSegId segId) override {
    }

    /**
     * Returns the next best peer from which to request product information.
     * @param[in] peer    The previous peer that failed
     * @param[in] prodId  The product identifier
     * @return            The next best peer. Might be invalid.
     */
    PeerPtr getAltPeer(
            const PeerPtr peer,
            const ProdId  prodId) override {
        return PeerPtr{};
    }

    /**
     * Returns the next best peer from which to request a data segment.
     * @param[in] peer    The previous peer that failed
     * @param[in] segId   The data segment ID
     * @return            The next best peer. Might be invalid.
     */
    PeerPtr getAltPeer(
            const PeerPtr   peer,
            const DataSegId segId) override {
        return PeerPtr{};
    }

    PeerPtr getWorstPeer() const override {
        PeerPtr      peer{};
        RequestCount maxCount = -1;
        Guard        guard(mutex);

        for (auto iter = numRequests.begin(), stop = numRequests.end(); iter != stop; ++iter) {
            const auto count = iter->second;

            if (count && count > maxCount) {
                maxCount = count;
                peer = iter->first;
            }
        }

        return peer;
    }

    /**
     * Resets all metrics.
     */
    void reset() noexcept override {
        Guard guard(mutex);

        for (auto iter = numRequests.begin(), stop = numRequests.end(); iter != stop; ++iter)
            iter->second = 0;
    }
};

BookkeeperPtr Bookkeeper::createPub(const int maxPeers) {
    return BookkeeperPtr{new PubBookkeeper(maxPeers)};
}

/******************************************************************************/

/**
 * Bookkeeper implementation for a subscriber
 */
class SubBookkeeper final : public BookkeeperImpl
{
    using Rating    = uint_fast32_t;
    using NoticeSet = std::set<Notice>;

    struct PeerInfo {
        Rating    rating;  ///< Peer rating
        NoticeSet notices; ///< Data available from remote peer
        PeerInfo()
            : rating(0)
            , notices()
        {}
    };

    /**
     * The following supports
     *   - Removal of associated data when the worst peer is removed;
     *   - Removal of associated peers when a datum is received; and
     *   - A peer with a given datum to be in the datum's list of peers at most
     *     once.
     */
    using PeerToInfo = std::unordered_map<PeerPtr, PeerInfo>;
    struct Peers {
        std::unordered_set<PeerPtr> set;
        std::list<PeerPtr>          list;
    };
    using NoticeToPeers = std::unordered_map<Notice, Peers>;

    /// Map of peer -> peer entry
    PeerToInfo peerToInfo;
    /**
     * Map of datum ID to peers that have the datum.
     */
    NoticeToPeers noticeToPeers;

    /**
     * Vets a peer.
     *
     * @pre                   `mutex` is locked
     * @param[in] peer        Peer to be vetted
     * @throw     LogicError  Peer is unknown
     */
    inline void vetPeer(PeerPtr peer) const {
        if (peerToInfo.count(peer) == 0)
            throw LOGIC_ERROR("Peer " + peer->to_string() + " is unknown");
    }

    /**
     * Indicates if a given, remote peer should be notified about an available datum.
     *
     * @param[in] peer        Local peer connected to remote peer
     * @param[in] notice      Notice of available datum
     * @return    true        Notice should be sent
     * @return    false       Notice shouldn't be sent
     * @throws    LogicError  Peer is unknown
     * @threadsafety          Safe
     * @cancellationpoint     No
     */
    bool shouldNotify(
            PeerPtr      peer,
            const Notice notice) const
    {
        Guard guard(mutex);

        vetPeer(peer);

        /*
         * The remote peer should be notified if no remote peer has announced that it
         * has the given datum or the given remote peer hasn't announced that it has.
         */
        return noticeToPeers.count(notice) == 0 ||
                noticeToPeers.at(notice).set.count(peer) == 0;
    }

    /**
     * Indicates if a local peer should request a datum from its remote peer. The peer is added to
     * the list of peers whose remote counterparts have the datum.
     *
     * @param[in] peer        Local peer
     * @param[in] notice      Notice of available data
     * @return    true        Request should be made
     * @return    false       Request shouldn't be made
     * @throws    LogicError  Peer is unknown
     * @threadsafety          Safe
     * @cancellationpoint     No
     */
    bool shouldRequest(
            PeerPtr      peer,
            const Notice notice)
    {
        Guard guard(mutex);

        vetPeer(peer);

        //LOG_DEBUG("Peer %s has datum %s", peer->to_string().data(),
                //notice.to_string().data());

        /*
         * A request for the given datum should be made if there are no peers
         * for the given datum currently.
         */
        bool  should = noticeToPeers.count(notice) == 0;
        /*
         * In any case, the given peer must now be added to the set of peers
         * that have the datum.
         */
        auto& datumPeers = noticeToPeers[notice];
        if (datumPeers.set.insert(peer).second)
            datumPeers.list.push_back(peer);

        return should;
    }

    /**
     * Process reception of a datum. The rating of the associated peer is increased.
     *
     * @param[in] peer        Peer that received the datum
     * @param[in] notice      Notice of available data
     * @retval    true        Success
     * @retval    false       Datum is unexpected
     * @threadsafety          Safe
     * @exceptionsafety       Strong guarantee
     * @cancellationpoint     No
     */
    bool received(PeerPtr      peer,
                  const Notice notice)
    {
        bool   success = false;
        Guard  guard(mutex);

        vetPeer(peer);

        if (noticeToPeers.count(notice) && noticeToPeers[notice].set.count(peer)) {
            ++(peerToInfo.at(peer).rating);
            success = true;
        }

        return success;
    }

    void erase(const Notice notice) {
        Guard  guard(mutex);
        if (noticeToPeers.count(notice)) {
            for (auto peer : noticeToPeers[notice].list)
                peerToInfo.at(peer).notices.erase(notice);
            noticeToPeers.erase(notice);
        }
    }

    /**
     * Returns the best alternative peer for a datum.
     *
     * @param[in] badPeer  Local peer whose remote couldn't supply the datum. Upon return, it will
     *                     not be in the the set of peers that indicated availability of the datum.
     * @param[in] notice   Notice of available data
     * @return             Best alternative peer. Will test false if it doesn't exist.
     */
    PeerPtr getAltPeer(
            const PeerPtr badPeer,
            const Notice  notice) {
        Guard   guard{mutex};
        PeerPtr altPeer;

        if (peerToInfo.count(badPeer))
            const auto count = peerToInfo[badPeer].notices.erase(notice);

        if (noticeToPeers.count(notice)) {
            noticeToPeers[notice].set.erase(badPeer);
            auto& peers = noticeToPeers[notice].list;
            if (peers.size()) {
                if (peers.front() == badPeer)
                    peers.pop_front();
                if (peers.size())
                    altPeer = peers.front();
            }
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
        , peerToInfo(maxPeers)
        , noticeToPeers()
    {}

    ~SubBookkeeper() noexcept {
        try {
            Guard guard(mutex);
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }
    }

    /**
     * Adds a peer.
     *
     * @param[in] peer            Peer
     * @retval true               Success
     * @retval false              Not added because already exists
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    bool add(PeerPtr peer) override {
        Guard guard(mutex);
        return peerToInfo.insert({peer, PeerInfo{}}).second;
    }

    /**
     * Removes a peer.
     *
     * @param[in] peer        The peer to be removed
     * @retval    true        Success
     * @retval    false       Peer is unknown
     * @threadsafety          Safe
     * @exceptionsafety       Basic guarantee
     * @cancellationpoint     No
     */
    bool erase(const PeerPtr peer) override
    {
        bool  existed = false;
        Guard guard{mutex};
        if (peerToInfo.count(peer)) {
            for (const auto& notice : peerToInfo[peer].notices) {
                auto& peers = noticeToPeers.at(notice);
                auto& peerList = peers.list;
                for (auto iter = peerList.begin(), end = peerList.end(); iter != end; ++iter) {
                    if (*iter == peer) {
                        peerList.erase(iter);
                        break;
                    }
                }
                peers.set.erase(peer);
            }
            peerToInfo.erase(peer);
            existed = true;
        }
        return existed;
    }

    void requested(const PeerPtr peer) override {
    }

    bool shouldNotify(
            PeerPtr      peer,
            const ProdId prodId) const override {
        return shouldNotify(peer, Notice(prodId));
    }

    bool shouldNotify(
            PeerPtr         peer,
            const DataSegId dataSegId) const override {
        return shouldNotify(peer, Notice(dataSegId));
    }

    bool shouldRequest(
            PeerPtr      peer,
            const ProdId prodId) override {
        return shouldRequest(peer, Notice(prodId));
    }

    bool shouldRequest(
            PeerPtr         peer,
            const DataSegId dataSegId) override {
        return shouldRequest(peer, Notice(dataSegId));
    }

    bool received(PeerPtr      peer,
                  const ProdId prodId) override {
        return received(peer, Notice{prodId});
    }

    /**
     * Process a peer having received a data segment. Nothing happens if it wasn't requested by the
     * peer; otherwise, the corresponding request is removed from the peer's outstanding requests.
     *
     * @param[in] peer        Peer
     * @param[in] segId       Data segment identifier
     * @retval    true        Success
     * @retval    false       Data segment wasn't requested
     * @threadsafety          Safe
     * @exceptionsafety       Strong guarantee
     * @cancellationpoint     No
     */
    bool received(PeerPtr         peer,
                  const DataSegId segId) override {
        return received(peer, Notice{segId});
    }

    void erase(const ProdId prodId) override {
        erase(Notice(prodId));
    }

    void erase(const DataSegId segId) override {
        erase(Notice(segId));
    }

    /**
     * Returns the best alternative peer, besides a given one, for requesting product information.
     * @param[in] peer    The peer that didn't receive the information
     * @param[in] prodId  The product's ID
     * @return
     */
    PeerPtr getAltPeer(
            const PeerPtr peer,
            const ProdId  prodId) override {
        return getAltPeer(peer, Notice{prodId});
    }

    /**
     * Returns the best alternative peer, besides a given one, for requesting a data segment.
     * @param[in] peer    The peer that didn't receive the segment
     * @param[in] segId   The segment's ID
     * @return
     */
    PeerPtr getAltPeer(
            const PeerPtr   peer,
            const DataSegId segId) override {
        return getAltPeer(peer, Notice{segId});
    }

    /**
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    PeerPtr getWorstPeer() const override
    {
        static PeerPtr noPeer{};
        PeerPtr        peer{};
        Rating         minRating = ~(Rating)0;
        bool           valid = false;
        Guard          guard(mutex);

        for (auto& elt : peerToInfo) {
            if (elt.first->isClient()) {
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

        for (auto& elt : peerToInfo)
            elt.second.rating = 0;
    }
};

BookkeeperPtr Bookkeeper::createSub(const int maxPeers) {
    return BookkeeperPtr{new SubBookkeeper(maxPeers)};
}

} // namespace
