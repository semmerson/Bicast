/**
 * Keeps track of peers and chunks in a thread-safe manner.
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
#include "Peer.h"

#include <climits>
#include <mutex>
#include <unordered_map>

namespace hycast {

/// Concurrency types:
typedef std::mutex             Mutex;
typedef std::lock_guard<Mutex> Guard;

/**
 * Implementation interface for performance monitoring of peers.
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

    virtual void add(const Peer& peer) =0;

    virtual Peer getWorstPeer() const =0;

    virtual void resetCounts() noexcept =0;

    virtual void erase(const Peer& peer) =0;
};

Bookkeeper::Bookkeeper(Impl* impl)
    : pImpl(impl) {
}

void Bookkeeper::add(const Peer& peer) const {
    pImpl->add(peer);
}

Peer Bookkeeper::getWorstPeer() const {
    return pImpl->getWorstPeer();
}

void Bookkeeper::resetCounts() const noexcept {
    pImpl->resetCounts();
}

void Bookkeeper::erase(const Peer& peer) const {
    pImpl->erase(peer);
}

/**
 * Bookkeeper implementation for a set of publisher-peers.
 */
class PubBookkeeper::Impl final : public Bookkeeper::Impl
{
    /// Map of peer -> number of chunks requested by remote peer
    std::unordered_map<Peer, uint_fast32_t> numRequested;

public:
    Impl(const int maxPeers)
        : Bookkeeper::Impl()
        , numRequested(maxPeers)
    {}

    void add(const Peer& peer) override {
        Guard guard(mutex);
        numRequested.insert({peer, 0});
    }

    void requested(
            const Peer&    peer,
            const ProdInfo prodInfo) {
        Guard guard(mutex);
        ++numRequested[peer];
    }

    void requested(
            const Peer&    peer,
            const SegInfo& segId) {
        Guard guard(mutex);
        ++numRequested[peer];
    }

    Peer getWorstPeer() const override {
        Peer          peer{};
        Guard         guard(mutex);

        if (numRequested.size() > 1) {
            unsigned long minCount{ULONG_MAX};

            for (auto& elt : numRequested) {
                auto count = elt.second;

                if (count < minCount) {
                    minCount = count;
                    peer = elt.first;
                }
            }
        }

        return peer;
    }

    void resetCounts() noexcept override {
        Guard guard(mutex);

        for (auto& elt : numRequested)
            elt.second = 0;
    }

    void erase(const Peer& peer) override {
        Guard guard(mutex);
        numRequested.erase(peer);
    }
};

PubBookkeeper::PubBookkeeper(const int maxPeers)
    : Bookkeeper(new Impl(maxPeers)) {
}

void PubBookkeeper::requested(
        const Peer&     peer,
        const ProdInfo& prodInfo) const {
    static_cast<Impl*>(pImpl.get())->requested(peer, prodInfo);
}

void PubBookkeeper::requested(const Peer& peer, const SegInfo& segInfo) const {
    static_cast<Impl*>(pImpl.get())->requested(peer, segInfo);
}

/**
 * Bookkeeper implementation for a set of subscriber-peers.
 */
class SubBookkeeper::Impl final : public Bookkeeper::Impl
{
    /// Information on a peer
    typedef struct PeerInfo {
        /// Requested chunks that haven't been received
        ChunkIds      reqChunks;
        uint_fast32_t chunkCount;  ///< Number of received chunks

        PeerInfo()
            : reqChunks()
            , chunkCount{0}
        {}
    } PeerInfo;

    /// Map of peer -> peer information
    std::unordered_map<Peer, PeerInfo> peerInfos;

    /// Map of chunk identifiers -> alternative peers that can request a chunk
    std::unordered_map<ChunkId, Peers> altPeers;

    /*
     * INVARIANT:
     *   - If `peerInfos[peer].reqChunks` contains `chunkId`, then `peer`
     *     is not contained in `altPeers[chunkId]`
     */

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
        , altPeers()
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
    void add(const Peer& peer) override
    {
        Guard guard(mutex);
        peerInfos.insert({peer, PeerInfo()});
    }

    /**
     * Returns the number of remote peers that are a path to the publisher of
     * data-products and the number that aren't.
     *
     * @param[out] numPath    Number of remote peers that are path to publisher
     * @param[out] numNoPath  Number of remote peers that aren't path to
     *                        publisher
     */
    void getSrcPathCounts(
            unsigned& numPath,
            unsigned& numNoPath) const
    {
        Guard guard(mutex);

        numPath = numNoPath = 0;

        for (auto& pair : peerInfos) {
            if (pair.first.isPathToPub()) {
                ++numPath;
            }
            else {
                ++numNoPath;
            }
        }
    }

    /**
     * Indicates if a chunk should be requested by a peer. If yes, then the
     * chunk is added to the list of chunks requested by the peer; if no, then
     * the peer is added to a list of potential peers for the chunk.
     *
     * @param[in] peer               Peer
     * @param[in] chunkId            Chunk Identifier
     * @return    `true`             Chunk should be requested
     * @return    `false`            Chunk shouldn't be requested
     * @throws    std::out_of_range  Remote peer is unknown
     * @throws    logicError         Chunk has already been requested from
     *                               remote peer or remote peer is already
     *                               alternative peer for chunk
     * @threadsafety                 Safe
     * @cancellationpoint            No
     */
    bool shouldRequest(
            Peer&          peer,
            const ChunkId  chunkId)
    {
        bool  should;
        Guard guard(mutex);
        auto  elt = altPeers.find(chunkId);

        if (elt == altPeers.end()) {
            // First request for this chunk
            auto& reqChunks = peerInfos.at(peer).reqChunks;

            // Check invariant
            if (reqChunks.find(chunkId) != reqChunks.end())
                throw LOGIC_ERROR("Peer " + peer.to_string() + " has "
                        "already requested chunk " + chunkId.to_string());

            altPeers[chunkId]; // Creates empty alternative-peer list
            // Add chunk to list of chunks requested by this peer
            reqChunks.insert(chunkId);
            should = true;
        }
        else {
            auto iter = peerInfos.find(peer);

            // Check invariant
            if (iter != peerInfos.end()) {
                auto& reqChunks = iter->second.reqChunks;
                if (reqChunks.find(chunkId) != reqChunks.end())
                    throw LOGIC_ERROR("Peer " + peer.to_string() + "requested "
                            "chunk " + chunkId.to_string());
            }

            elt->second.push_back(peer); // Add alternative peer for this chunk
            should = false;
        }

        //LOG_DEBUG("Chunk %s %s be requested from %s", chunkId.to_string().data(),
                //should ? "should" : "shouldn't", peer.to_string().data());
        return should;
    }

    /**
     * Process a chunk as having been received from a peer. Nothing happens if
     * the chunk wasn't requested by the peer; otherwise, the peer is marked as
     * having received the chunk and the set of alternative peers that could but
     * haven't requested the chunk is cleared.
     *
     * @param[in] peer               Peer
     * @param[in] chunkId            Chunk Identifier
     * @retval    `false`            Chunk wasn't requested by peer.
     * @retval    `true`             Chunk was requested by peer
     * @throws    std::out_of_range  `peer` is unknown
     * @threadsafety                 Safe
     * @exceptionsafety              Basic guarantee
     * @cancellationpoint            No
     */
    bool received(
            Peer&         peer,
            const ChunkId chunkId)
    {
        Guard  guard(mutex);
        auto&  peerInfo = peerInfos.at(peer);
        bool   wasRequested;

        if (peerInfo.reqChunks.erase(chunkId) == 0) {
            wasRequested = false;
        }
        else {
            ++peerInfo.chunkCount;
            altPeers.erase(chunkId); // Chunk is no longer relevant
            wasRequested = true;
        }

        return wasRequested;
    }

    /**
     * Returns a worst performing peer.
     *
     * @return                    A worst performing peer since construction
     *                            or `resetCounts()` was called. Will test
     *                            false if the set is empty.
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    Peer getWorstPeer() const override
    {
        unsigned long minCount{ULONG_MAX};
        Peer          peer{};
        Guard         guard(mutex);

        for (auto& elt : peerInfos) {
            auto count = elt.second.chunkCount;

            if (count < minCount) {
                minCount = count;
                peer = elt.first;
            }
        }

        return peer;
    }

    /**
     * Returns a worst performing peer.
     *
     * @param[in] isPathToSrc     Attribute that peer must have
     * @return                    A worst performing peer -- whose
     *                            `isPathToSrc()` return value equals
     *                            `isPathToSrc` -- since construction or
     *                            `resetCounts()` was called. Will test false if
     *                            the set is empty.
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    Peer getWorstPeer(const bool isPathToSrc) const
    {
        Peer          peer{};
        Guard         guard(mutex);

        if (peerInfos.size() > 1) {
            unsigned long minCount{ULONG_MAX};

            for (auto elt : peerInfos) {
                if (elt.first.isPathToPub() == isPathToSrc) {
                    auto count = elt.second.chunkCount;

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
    void resetCounts() noexcept override
    {
        Guard guard(mutex);

        for (auto& elt : peerInfos)
            elt.second.chunkCount = 0;
    }

    /**
     * Returns a reference to the identifiers of chunks that a peer has
     * requested but that have not yet been received. The set of identifiers
     * is deleted when `erase()` is called -- so the reference must not be
     * dereferenced after that.
     *
     * @param[in] peer            The peer in question
     * @return                    [first, last) iterators over the chunk
     *                            identifiers
     * @throws std::out_of_range  `peer` is unknown
     * @validity                  No changes to the peer's account
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     * @see                       `popBestAlt()`
     * @see                       `requested()`
     * @see                       `erase()`
     */
    const ChunkIds& getRequested(const Peer& peer) const
    {
        Guard guard(mutex);
        return peerInfos.at(peer).reqChunks;
    }

    /**
     * Returns the best peer to request a chunk that hasn't already requested
     * it. The peer is removed from the set of such peers.
     *
     * @param[in] chunkId         Chunk Identifier
     * @return                    The peer. Will test `false` if no such peer
     *                            exists.
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     * @see                       `getRequested()`
     * @see                       `requested()`
     * @see                       `erase()`
     */
    Peer popBestAlt(const ChunkId chunkId)
    {
        Guard guard(mutex);
        Peer  peer{};
        auto  iter = altPeers.find(chunkId);

        if (iter != altPeers.end()) {
            auto& peers = iter->second;
            if (!peers.empty()) {
                peer = peers.front();
                peers.pop_front();
            }
        }

        return peer;
    }

    /**
     * Marks a peer as being responsible for a chunk.
     *
     * @param[in] peer     Peer
     * @param[in] chunkId  Identifier of chunk
     * @see                `getRequested()`
     * @see                `popBestAlt()`
     * @see                `erase()`
     */
    void requested(
            const Peer&   peer,
            const ChunkId chunkId)
    {
        Guard guard{mutex};
        peerInfos[peer].reqChunks.insert(chunkId);
    }

    /**
     * Removes a peer. Should be called after processing the entire set
     * returned by `getChunkIds()`.
     *
     * @param[in] peer            The peer to be removed
     * @throws std::out_of_range  `peer` is unknown
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     * @see                       `getRequested()`
     * @see                       `popBestAlt()`
     * @see                       `requested()`
     */
    void erase(const Peer& peer) override
    {
        Guard    guard(mutex);

        for (auto& elt : altPeers)
            elt.second.remove(peer);

        peerInfos.erase(peer);
    }
};

SubBookkeeper::SubBookkeeper(const int maxPeers)
    : Bookkeeper(new Impl(maxPeers)) {
}

void SubBookkeeper::getPubPathCounts(
        unsigned& numPath,
        unsigned& numNoPath) const {
    static_cast<Impl*>(pImpl.get())->getSrcPathCounts(numPath,
            numNoPath);
}

bool SubBookkeeper::shouldRequest(
        Peer&         peer,
        const ChunkId chunkId) const {
    return static_cast<Impl*>(pImpl.get())->shouldRequest(peer,
            chunkId);
}

bool SubBookkeeper::received(
        Peer&         peer,
        const ChunkId chunkId) const {
    return static_cast<Impl*>(pImpl.get())->received(peer, chunkId);
}

Peer SubBookkeeper::getWorstPeer(const bool isPathToSrc) const {
    return static_cast<Impl*>(pImpl.get())->getWorstPeer(isPathToSrc);
}

const SubBookkeeper::ChunkIds&
SubBookkeeper::getRequested(const Peer& peer) const {
    return static_cast<Impl*>(pImpl.get())->getRequested(peer);
}

Peer SubBookkeeper::popBestAlt(const ChunkId chunkId) const {
    return static_cast<Impl*>(pImpl.get())->popBestAlt(chunkId);
}

void SubBookkeeper::requested(
        const Peer&    peer,
        const ChunkId& chunkId) const {
    static_cast<Impl*>(pImpl.get())->requested(peer, chunkId);
}

} // namespace
