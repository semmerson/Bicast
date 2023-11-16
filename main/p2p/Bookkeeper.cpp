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
#include "BicastProto.h"
#include "logging.h"
#include "Notice.h"
#include "RunPar.h"
#include "Trigger.h"

#include <climits>
#include <list>
#include <mutex>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace bicast {

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

    /// Map of remote peer address -> number of requests by remote peer
    std::unordered_map<SockAddr, RequestCount> numRequests;

public:
    /**
     * Constructs.
     */
    PubBookkeeper()
        : BookkeeperImpl()
        , numRequests(RunPar::maxNumPeers)
    {}

    bool add(
            const SockAddr& rmtAddr,
            const bool      isClient) override {
        Guard guard(mutex);
        return numRequests.insert({rmtAddr, 0}).second;
    }

    bool erase(const SockAddr& rmtAddr) override {
        Guard guard(mutex);
        return numRequests.erase(rmtAddr) == 1;
    }

    void requested(const SockAddr& rmtAddr) override {
        Guard guard(mutex);
        ++numRequests[rmtAddr];
    }

    bool shouldNotify(
            const SockAddr& rmtAddr,
            const ProdId    prodId) const override {
        return false;
    }

    bool shouldNotify(
            const SockAddr& rmtAddr,
            const DataSegId dataSegId) const override {
        return false;
    }

    bool shouldRequest(
            const SockAddr& rmtAddr,
            const ProdId    prodindex) override {
        return false;
    }

    bool shouldRequest(
            const SockAddr& rmtAddr,
            const DataSegId dataSegId) override {
        return false;
    }

    bool received(
            const SockAddr& rmtAddr,
            const ProdId    prodId) override {
        return false;
    }

    bool received(
            const SockAddr& rmtAddr,
            const DataSegId segId) override {
        return false;
    }

    void erase(const ProdId prodId) override {
    }

    void erase(const DataSegId segId) override {
    }

    SockAddr getAltPeer(
            const SockAddr& rmtAddr,
            const ProdId    prodId) override {
        return SockAddr{};
    }

    SockAddr getAltPeer(
            const SockAddr& rmtAddr,
            const DataSegId segId) override {
        return SockAddr{};
    }

    SockAddr getWorstPeer() const override {
        SockAddr     rmtAddr{};
        RequestCount maxCount = -1;
        Guard        guard(mutex);

        for (auto iter = numRequests.begin(), stop = numRequests.end(); iter != stop; ++iter) {
            const auto count = iter->second;

            if (count && count > maxCount) {
                maxCount = count;
                rmtAddr = iter->first;
            }
        }

        return rmtAddr;
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

BookkeeperPtr Bookkeeper::createPub() {
    return BookkeeperPtr{new PubBookkeeper()};
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
        Rating     rating;   ///< Peer rating
        NoticeSet  notices;  ///< Data available from remote peer
        const bool isClient; ///< Local peer was constructed client-side (i.e., it initiated the
                             ///< connection
        explicit PeerInfo(const bool isClient = false)
            : rating(0)
            , notices()
            , isClient(isClient)
        {}
    };

    /**
     * The following supports
     *   - Removal of associated data when the worst peer is removed;
     *   - Removal of associated peers when a datum is received; and
     *   - A peer with a given datum to be in the datum's list of peers at most
     *     once.
     */
    using PeerToInfo = std::unordered_map<SockAddr, PeerInfo>;
    struct Peers {
        std::unordered_set<SockAddr> set;
        std::list<SockAddr>          list;
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
     * @param[in] rmtAddr     Socket address of the remote peer
     * @throw     LogicError  Peer is unknown
     */
    inline void vetPeer(const SockAddr& rmtAddr) const {
        if (peerToInfo.count(rmtAddr) == 0)
            throw LOGIC_ERROR("Peer " + rmtAddr.to_string() + " is unknown");
    }

    /**
     * Indicates if a given, remote peer should be notified about an available datum.
     *
     * @param[in] rmtAddr     Socket address of the remote peer
     * @param[in] notice      Notice of available datum
     * @return    true        Notice should be sent
     * @return    false       Notice shouldn't be sent
     * @throws    LogicError  Peer is unknown
     * @threadsafety          Safe
     * @cancellationpoint     No
     */
    bool shouldNotify(
            const SockAddr& rmtAddr,
            const Notice    notice) const
    {
        Guard guard(mutex);

        vetPeer(rmtAddr);

        /*
         * The remote rmtAddr should be notified if no remote rmtAddr has announced that it
         * has the given datum or the given remote peer hasn't announced that it has.
         */
        return noticeToPeers.count(notice) == 0 ||
                noticeToPeers.at(notice).set.count(rmtAddr) == 0;
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
            const SockAddr& rmtAddr,
            const Notice    notice)
    {
        Guard guard(mutex);

        vetPeer(rmtAddr);

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
        if (datumPeers.set.insert(rmtAddr).second)
            datumPeers.list.push_back(rmtAddr);

        return should;
    }

    /**
     * Process reception of a datum. The rating of the associated peer is increased.
     *
     * @param[in] rmtAddr     Socket address of the remote peer
     * @param[in] notice      Notice of available data
     * @retval    true        Success
     * @retval    false       Datum is unexpected
     * @threadsafety          Safe
     * @exceptionsafety       Strong guarantee
     * @cancellationpoint     No
     */
    bool received(
            const SockAddr& rmtAddr,
            const Notice    notice)
    {
        bool   success = false;
        Guard  guard(mutex);

        vetPeer(rmtAddr);

        if (noticeToPeers.count(notice) && noticeToPeers[notice].set.count(rmtAddr)) {
            ++(peerToInfo.at(rmtAddr).rating);
            success = true;
        }

        return success;
    }

    void erase(const Notice notice) {
        Guard  guard(mutex);
        if (noticeToPeers.count(notice)) {
            for (auto rmtAddr : noticeToPeers[notice].list)
                peerToInfo.at(rmtAddr).notices.erase(notice);
            noticeToPeers.erase(notice);
        }
    }

    /**
     * Returns the best alternative peer for a datum.
     *
     * @param[in] rmtAddr  Socket address of remote peer that couldn't supply the datum. Upon
     *                     return, it will not be in the the set of peers that indicated
     *                     availability of the datum.
     * @param[in] notice   Notice of available data
     * @return             Socket address of best alternative, remote peer. Will test false if it
     *                     doesn't exist.
     */
    SockAddr getAltPeer(
            const SockAddr& rmtAddr,
            const Notice    notice) {
        Guard    guard{mutex};
        SockAddr altRmtAddr{};

        if (peerToInfo.count(rmtAddr))
            const auto count = peerToInfo[rmtAddr].notices.erase(notice);

        if (noticeToPeers.count(notice)) {
            noticeToPeers[notice].set.erase(rmtAddr);
            auto& peers = noticeToPeers[notice].list;
            if (peers.size()) {
                if (peers.front() == rmtAddr)
                    peers.pop_front();
                if (peers.size())
                    altRmtAddr = peers.front();
            }
        }

        return altRmtAddr;
    }

public:
    /**
     * Constructs.
     *
     * @throws std::system_error  Out of memory
     * @cancellationpoint         No
     */
    SubBookkeeper()
        : BookkeeperImpl()
        , peerToInfo(RunPar::maxNumPeers)
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

    bool add(
            const SockAddr& rmtAddr,
            const bool      isClient) override {
        Guard guard(mutex);
        return peerToInfo.insert({rmtAddr, PeerInfo{isClient}}).second;
    }

    bool erase(const SockAddr& rmtAddr) override
    {
        bool  existed = false;
        Guard guard{mutex};
        if (peerToInfo.count(rmtAddr)) {
            for (const auto& notice : peerToInfo[rmtAddr].notices) {
                auto& peers = noticeToPeers.at(notice);
                auto& peerList = peers.list;
                for (auto iter = peerList.begin(), end = peerList.end(); iter != end; ++iter) {
                    if (*iter == rmtAddr) {
                        peerList.erase(iter);
                        break;
                    }
                }
                peers.set.erase(rmtAddr);
            }
            peerToInfo.erase(rmtAddr);
            existed = true;
        }
        return existed;
    }

    void requested(const SockAddr& rmtAddr) override {
    }

    bool shouldNotify(
            const SockAddr& rmtAddr,
            const ProdId    prodId) const override {
        return shouldNotify(rmtAddr, Notice(prodId));
    }

    bool shouldNotify(
            const SockAddr& rmtAddr,
            const DataSegId dataSegId) const override {
        return shouldNotify(rmtAddr, Notice(dataSegId));
    }

    bool shouldRequest(
            const SockAddr& rmtAddr,
            const ProdId    prodId) override {
        return shouldRequest(rmtAddr, Notice(prodId));
    }

    bool shouldRequest(
            const SockAddr& rmtAddr,
            const DataSegId dataSegId) override {
        return shouldRequest(rmtAddr, Notice(dataSegId));
    }

    bool received(
            const SockAddr& rmtAddr,
            const ProdId    prodId) override {
        return received(rmtAddr, Notice{prodId});
    }

    bool received(
            const SockAddr& rmtAddr,
            const DataSegId segId) override {
        return received(rmtAddr, Notice{segId});
    }

    void erase(const ProdId prodId) override {
        erase(Notice(prodId));
    }

    void erase(const DataSegId segId) override {
        erase(Notice(segId));
    }

    SockAddr getAltPeer(
            const SockAddr& rmtAddr,
            const ProdId    prodId) override {
        return getAltPeer(rmtAddr, Notice{prodId});
    }

    SockAddr getAltPeer(
            const SockAddr& rmtAddr,
            const DataSegId segId) override {
        return getAltPeer(rmtAddr, Notice{segId});
    }

    SockAddr getWorstPeer() const override
    {
        static SockAddr noPeer{};
        SockAddr        rmtAddr{};
        Rating          minRating = ~(Rating)0;
        bool            valid = false;
        Guard           guard(mutex);

        for (auto& elt : peerToInfo) {
            if (elt.second.isClient) {
                const auto rating = elt.second.rating;
                if (rating)
                    valid = true;
                if (rating < minRating) {
                    minRating = rating;
                    rmtAddr = elt.first;
                }
            }
        }

        return valid ? rmtAddr : noPeer;
    }

    void reset() noexcept override {
        Guard guard(mutex);

        for (auto& elt : peerToInfo)
            elt.second.rating = 0;
    }
};

BookkeeperPtr Bookkeeper::createSub() {
    return BookkeeperPtr{new SubBookkeeper()};
}

} // namespace
