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

    virtual void add(const Peer peer) =0;

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

    bool remove(const Peer peer) override {
        Guard guard(mutex);
        return numRequests.erase(peer) == 1;
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

bool PubBookkeeper::remove(const Peer peer) const {
    return static_cast<Impl*>(pImpl.get())->remove(peer);
}

/******************************************************************************/

/**
 * Bookkeeper implementation for a subscriber
 */
class SubBookkeeper::Impl final : public Bookkeeper::Impl
{
    using Ratings    = std::unordered_map<Peer, uint_fast32_t>;
    using PeerQueue  = std::queue<Peer, std::list<Peer>>;
    using PeerQueues = std::unordered_map<NoteReq, PeerQueue>;

    /// Map of peer -> peer rating
    Ratings    ratings;
    /**
     * Map of request -> alternative peers that could make the request in the
     * order in which their notifications arrived.
     */
    PeerQueues altPeers;

    /**
     * Indicates if a request should be made by a peer. if not, then the peer is
     * added to the queue of alternative peers for the request.
     *
     * @param[in] peer        Peer
     * @param[in] request     Request
     * @return    `true`      Request should be made
     * @return    `false`     Request shouldn't be made
     * @throws    LogicError  Peer is unknown
     * @threadsafety          Safe
     * @cancellationpoint     No
     */
    bool shouldRequest(
            Peer           peer,
            const NoteReq& request)
    {
        Guard guard(mutex);

        if (ratings.count(peer) == 0)
            throw LOGIC_ERROR("Peer " + peer.to_string() + " is unknown");

        const bool should = altPeers.count(request) == 0;

        if (should) {
            altPeers[request] = PeerQueue{}; // NB: `peer` not in empty queue
        }
        else {
            altPeers[request].push(peer); // Add alternative peer. Might throw.
        }

        return should;
    }

    /**
     * Process a satisfied request. The rating of the associated peer is
     * increased and the set of alternative peers that could make the request is
     * cleared.
     *
     * @param[in] peer        Peer
     * @param[in] request     Request
     * @throws    LogicError  Peer is unknown
     * @throws    LogicError  Request is unknown
     * @threadsafety          Safe
     * @exceptionsafety       Basic guarantee
     * @cancellationpoint     No
     */
    void received(Peer           peer,
                  const NoteReq& request)
    {
        Guard  guard(mutex);

        if (ratings.count(peer) == 0)
            throw LOGIC_ERROR("Peer " + peer.to_string() + " is unknown");
        if (altPeers.count(request) == 0)
            throw LOGIC_ERROR("Request " + request.to_string() + " is unknown");

        ++ratings.at(peer);
        altPeers.erase(request); // No longer relevant
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
        , altPeers(8)
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
        ratings.insert({peer, 0});
    }

    bool shouldRequest(Peer peer, const ProdIndex prodIndex) {
        return shouldRequest(peer, NoteReq(prodIndex));
    }

    bool shouldRequest(Peer peer, const DataSegId& dataSegId) {
        return shouldRequest(peer, NoteReq(dataSegId));
    }

    void received(Peer            peer,
                  const ProdIndex prodIndex) {
        received(peer, NoteReq{prodIndex});
    }

    void received(Peer             peer,
                  const DataSegId& dataSegId) {
        received(peer, NoteReq{dataSegId});
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

        for (auto& pair : ratings) {
            if (pair.first.rmtIsPubPath()) {
                ++numPath;
            }
            else {
                ++numNoPath;
            }
        }
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

void SubBookkeeper::received(
        Peer            peer,
        const ProdIndex prodIndex) const {
    static_cast<Impl*>(pImpl.get())->received(peer, prodIndex);
}

void SubBookkeeper::received(
        Peer             peer,
        const DataSegId& dataSegId) const {
    static_cast<Impl*>(pImpl.get())->received(peer, dataSegId);
}

Peer SubBookkeeper::getWorstPeer(const bool isClient) const {
    return static_cast<Impl*>(pImpl.get())->getWorstPeer(isClient);
}

bool SubBookkeeper::remove(const Peer peer) const {
    return static_cast<Impl*>(pImpl.get())->remove(peer);
}

} // namespace
