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

    virtual void add(const Peer peer) =0;

    virtual void reset() noexcept =0;

    virtual void erase(const Peer peer) =0;
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

    void add(const Peer peer) override {
        Guard guard(mutex);
        numRequests[peer] = 0;
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

    void erase(const Peer peer) override {
        Guard guard(mutex);
        numRequests.erase(peer);
    }
};

PubBookkeeper::PubBookkeeper(const int maxPeers)
    : Bookkeeper(new Impl(maxPeers)) {
}

void PubBookkeeper::add(const Peer peer) const {
    static_cast<Impl*>(pImpl.get())->add(peer);
}

void PubBookkeeper::requested(const Peer peer) const {
    static_cast<Impl*>(pImpl.get())->requested(peer);
}

Peer PubBookkeeper::getWorstPeer() const {
    return static_cast<Impl*>(pImpl.get())->getWorstPeer();
}

void PubBookkeeper::reset() const noexcept {
    static_cast<Impl*>(pImpl.get())->reset();
}

void PubBookkeeper::erase(const Peer peer) const {
    static_cast<Impl*>(pImpl.get())->erase(peer);
}

/******************************************************************************/

/**
 * Bookkeeper implementation for a subscriber
 */
class SubBookkeeper::Impl final : public Bookkeeper::Impl
{
    struct Request
    {
        PduId pduId;
        union {
            ProdIndex prodIndex;
            DataSegId dataSegId;
        };

        Request(ProdIndex prodIndex)
            : pduId(PduId::PROD_INFO_REQUEST)
            , prodIndex(prodIndex)
        {}
        Request(DataSegId dataSegId)
            : pduId(PduId::DATA_SEG_REQUEST)
            , dataSegId(dataSegId)
        {}
        String to_string() const {
            return (pduId == PduId::PROD_INFO_REQUEST)
                    ? prodIndex.to_string()
                    : dataSegId.to_string();
        }
        void beRequestedBy(Peer peer) const {
            (pduId == PduId::PROD_INFO_REQUEST)
                ? peer.request(prodIndex)
                : peer.request(dataSegId);
        }
        size_t hash() const noexcept {
            return (pduId == PduId::PROD_INFO_REQUEST)
                    ? prodIndex.hash()
                    : dataSegId.hash();
        }
        bool operator==(const Request& rhs) const noexcept {
            return (pduId == rhs.pduId) &&
                    ((pduId == PduId::PROD_INFO_REQUEST)
                        ? prodIndex == rhs.prodIndex
                        : dataSegId == rhs.dataSegId);
        }
    };

    struct RequestHash {
        size_t operator()(const Request& request) const noexcept {
            return request.hash();
        }
    };

    using RequestSet = std::unordered_set<Request, RequestHash>;

    /// Information on a peer
    struct PeerInfo {
        /// Outstanding requests that haven't been satisfied yet
        RequestSet    requests;
        uint_fast32_t count;  ///< Number of received PDU-s

        PeerInfo()
            : requests(100)
            , count{0}
        {}
    };

    class PeerQueue {
        std::list<Peer> peers;

    public:
        PeerQueue() =default;

        /**
         * Appends a peer to the queue.
         *
         * @param[in] peer          Peer to append
         * @throw std::logic_error  Peer is already in queue
         */
        void append(Peer peer) {
            for (const auto& elt : peers)
                if (elt == peer)
                    throw LOGIC_ERROR("Peer " + peer.to_string() + " is "
                            "already in the queue");
            peers.push_back(peer);
        }

        Peer pop() {
            Peer peer{};
            if (!peers.empty()) {
                peer = peers.front();
                peers.pop_front();
            }
            return peer;
        }
    };

    using PeerInfos = std::unordered_map<Peer, PeerInfo>;
    using ReqPeers  = std::unordered_map<Request, PeerQueue, RequestHash>;

    /// Map of peer -> peer information
    PeerInfos peerInfos;
    /**
     * Map of request -> peers that can make the request in the order in which
     * their notifications arrived.
     */
    ReqPeers  reqPeers;

    /**
     * Indicates if a request should be made by a peer. If yes, then the
     * request is added to the list of requests by the peer; if no, then
     * the peer is added to a list of potential peers for the request.
     *
     * @param[in] peer               Peer
     * @param[in] request            Request
     * @return    `true`             Request should be made
     * @return    `false`            Request shouldn't be made
     * @throws    logic_error        Peer is not in the set
     * @threadsafety                 Safe
     * @cancellationpoint            No
     */
    bool shouldRequest(
            Peer           peer,
            const Request& request)
    {
        Guard guard(mutex);
        bool  should = reqPeers.count(request) == 0;

        if (should) {
            try {
                auto& peerRequests = peerInfos.at(peer).requests;

                LOG_ASSERT(peerRequests.count(request) == 0);

                peerRequests.insert(request); // Add request to peer

                should = true;
            }
            catch (const std::out_of_range& ex) {
                std::throw_with_nested(LOGIC_ERROR("Peer " + peer.to_string() +
                        " is not in the set"));
            }
        }

        reqPeers[request].append(peer); // Add peer to request. Throws

        return should;
    }

    /**
     * Process a satisfied request. Nothing happens if the peer didn't make the
     * request; otherwise, the request is deleted from the peer's list of
     * outstanding requests and the set of alternative peers that could make the
     * request is cleared.
     *
     * @param[in] peer               Peer
     * @param[in] request            Request
     * @retval    `false`            Peer didn't make request
     * @retval    `true`             Peer made request
     * @throws    logic_error        Peer is not in the set
     * @threadsafety                 Safe
     * @exceptionsafety              Basic guarantee
     * @cancellationpoint            No
     */
    bool received(Peer          peer,
                  const Request request)
    {
        Guard  guard(mutex);
        bool   wasRequested;

        try {
            auto& peerInfo = peerInfos.at(peer);

            if (peerInfo.requests.erase(request) == 0) {
                // Peer didn't make request
                wasRequested = false;
            }
            else {
                // Peer made request
                ++peerInfo.count;
                wasRequested = true;
            }

            reqPeers.erase(request); // No longer relevant
        }
        catch (const std::out_of_range& ex) {
            std::throw_with_nested(LOGIC_ERROR("Peer " + peer.to_string() +
                    " is not in the set"));
        }

        return wasRequested;
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
        , peerInfos(maxPeers)
        , reqPeers(8)
    {}

    /**
     * Adds a peer.
     *
     * @param[in] peer            Peer
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    void add(Peer peer) override
    {
        Guard guard(mutex);
        peerInfos.insert({peer, PeerInfo()});
    }

    bool shouldRequest(Peer peer, const ProdIndex prodIndex) {
        return shouldRequest(peer, Request(prodIndex));
    }

    bool shouldRequest(Peer peer, const DataSegId& dataSegId) {
        return shouldRequest(peer, Request(dataSegId));
    }

    bool received(Peer            peer,
                  const ProdIndex prodIndex) {
        return received(peer, Request(prodIndex));
    }

    bool received(Peer             peer,
                  const DataSegId& dataSegId) {
        return received(peer, Request(dataSegId));
    }

    /**
     * Returns the number of remote peers that are a path to the publisher and
     * the number that aren't.
     *
     * @param[out] numPath    Number of remote peers that are path to publisher
     * @param[out] numNoPath  Number of remote peers that aren't path to
     *                        publisher
     */
    void getPubPathCounts(
            unsigned& numPath,
            unsigned& numNoPath) const
    {
        Guard guard(mutex);

        numPath = numNoPath = 0;

        for (auto& pair : peerInfos) {
            if (pair.first.rmtIsPubPath()) {
                ++numPath;
            }
            else {
                ++numNoPath;
            }
        }
    }

    /**
     * Returns a worst performing peer.
     *
     * @param[in] pubPath         Attribute that peer must have
     * @return                    A worst performing peer -- whose
     *                            `rmtPubPath()` return value equals `pubPath`
     *                            -- since construction or `reset()` was called.
     *                            Will test false if the set is empty.
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    Peer getWorstPeer(const bool pubPath) const
    {
        Peer  peer{};
        Guard guard(mutex);

        if (peerInfos.size() > 1) {
            unsigned long minCount{ULONG_MAX};

            for (auto elt : peerInfos) {
                if (elt.first.rmtIsPubPath() == pubPath) {
                    auto count = elt.second.count;

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
     * Resets the count of received chunks for every peer.
     *
     * @threadsafety       Safe
     * @exceptionsafety    No throw
     * @cancellationpoint  No
     */
    void reset() noexcept override
    {
        Guard guard(mutex);

        for (auto& elt : peerInfos)
            elt.second.count = 0;
    }

    /**
     * Removes a peer. The peer's unsatisfied requests are transferred to
     * alternative peers.
     *
     * @param[in] peer            The peer to be removed
     * @throws    logic_error     Peer is not in set
     * @throws    logic_error     Alternative peer is not in set
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    void erase(const Peer peer) override
    {
        Guard guard(mutex);

        try {
            for (const auto& outRequest : peerInfos.at(peer).requests) {
                auto& peers = reqPeers[outRequest];

                auto  altPeer = peers.pop();
                LOG_ASSERT(altPeer == peer);
                altPeer = peers.pop();

                if (altPeer) {
                    try {
                        peerInfos.at(altPeer).requests.insert(outRequest);
                        outRequest.beRequestedBy(altPeer);
                    }
                    catch (const std::out_of_range& ex) {
                        std::throw_with_nested(LOGIC_ERROR("Alternative peer "
                                + altPeer.to_string() + " is not in the set"));
                    }
                }
            }
        }
        catch (const std::out_of_range& ex) {
            std::throw_with_nested(LOGIC_ERROR("Peer " + peer.to_string() +
                    " is not in the set"));
        }

        peerInfos.erase(peer);
    }
};

SubBookkeeper::SubBookkeeper(const int maxPeers)
    : Bookkeeper(new Impl(maxPeers)) {
}

void SubBookkeeper::add(const Peer peer) const {
    static_cast<Impl*>(pImpl.get())->add(peer);
}

void SubBookkeeper::getPubPathCounts(
        unsigned& numPath,
        unsigned& numNoPath) const {
    static_cast<Impl*>(pImpl.get())->getPubPathCounts(numPath, numNoPath);
}

bool SubBookkeeper::shouldRequest(
        Peer            peer,
        const ProdIndex prodIndex) const {
    return static_cast<Impl*>(pImpl.get())->shouldRequest(peer, prodIndex);
}

bool SubBookkeeper::shouldRequest(
        Peer             peer,
        const DataSegId& dataSegId) const {
    return static_cast<Impl*>(pImpl.get())->shouldRequest(peer, dataSegId);
}

bool SubBookkeeper::received(
        Peer            peer,
        const ProdIndex prodIndex) const {
    return static_cast<Impl*>(pImpl.get())->received(peer, prodIndex);
}

bool SubBookkeeper::received(
        Peer             peer,
        const DataSegId& dataSegId) const {
    return static_cast<Impl*>(pImpl.get())->received(peer, dataSegId);
}

Peer SubBookkeeper::getWorstPeer(const bool pubPath) const {
    return static_cast<Impl*>(pImpl.get())->getWorstPeer(pubPath);
}

void SubBookkeeper::reset() const noexcept {
    static_cast<Impl*>(pImpl.get())->reset();
}

void SubBookkeeper::erase(const Peer peer) const {
    static_cast<Impl*>(pImpl.get())->erase(peer);
}

} // namespace
