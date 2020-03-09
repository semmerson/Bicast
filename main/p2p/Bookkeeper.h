/**
 * Keeps track of peers and message exchanges in a thread-safe manner.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Bookkeeper.h
 *  Created on: Oct 17, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_P2P_BOOKKEEPER_H_
#define MAIN_P2P_BOOKKEEPER_H_

#include "Peer.h"

#include <list>
#include <memory>
#include <unordered_set>
#include <utility>

namespace hycast {

class Bookkeeper
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    typedef std::unordered_set<ChunkId> ChunkIds;
    typedef std::list<Peer>             Peers;
    typedef ChunkIds::iterator          ChunkIdIter;
    typedef Peers::iterator             PeerIter;

    /**
     * Constructs.
     *
     * @param[in] maxPeers        Maximum number of peers
     * @throws std::system_error  Out of memory
     * @cancellationpoint         No
     */
    Bookkeeper(int maxPeers);

    /**
     * Adds a peer.
     *
     * @param[in] peer            The peer
     * @param[in] fromConnect     Did the peer result from `::connect()`?
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    void add(
            const Peer& peer,
            const bool  fromConnect) const;

    /**
     * Indicates if a peer resulted from a call to `::connect()`.
     *
     * @param[in] peer            The peer
     * @retval    `true`          The peer did result from a call to
     *                            `::connect()`
     * @retval    `false`         The peer did not result from a call to
     *                            `::connect()`
     * @throws std::out_of_range  `peer` is unknown
     * @threadsafety              Safe
     * @exceptionsafety           Strong guarantee
     * @cancellationpoint         No
     */
    bool isFromConnect(const Peer& peer) const;

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
            Peer&           peer,
            const ChunkId   chunkId) const;

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
            Peer&           peer,
            const ChunkId   chunkId) const;

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
    Peer getWorstPeer() const;

    /**
     * Resets the measure of utility for every peer.
     *
     * @threadsafety       Safe
     * @exceptionsafety    No throw
     * @cancellationpoint  No
     */
    void resetCounts() const noexcept;

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
    Bookkeeper::ChunkIds& getRequested(const Peer& peer) const;

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
    Peer popBestAlt(const ChunkId chunkId) const;

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
            Peer&           peer,
            const ChunkId   chunkId) const;

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
    void erase(const Peer& peer) const;
};

} // namespace

#endif /* MAIN_P2P_BOOKKEEPER_H_ */
