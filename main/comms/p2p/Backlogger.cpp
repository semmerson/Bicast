/**
 * This file implements a class that retrieves the backlog of data-products when
 * a peer first connects to its remote peer.
 *
 * An instance of this class needs the following:
 *   - Access to the product-store
 *   - Identity of the chunk of data with which to start
 *   - Identity of the chunk of data at which to stop
 *   - The relevant local peer
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Backlogger.cpp
 *  Created on: Sep 21, 2017
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "Thread.h"

#include "Backlogger.h"

namespace hycast {

class Backlogger::Impl
{
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  LockGuard;
    typedef std::unique_lock<Mutex> UniqueLock;
    typedef std::condition_variable Cond;

    mutable Mutex mutex;
    Cond          cond;
    Peer          peer;
    ChunkInfo     startWith;
    ChunkInfo     stopAt;
    ProdStore     prodStore;

    /**
     * Returns the identify of the chunk of data at which the sending of
     * backlog notices to the remote peer should stop. Blocks until
     * `doNotNotifyOf()` has been called with a non-empty `ChunkInfo`.
     * @return  Identity of data-chunk at which backlog notices should stop
     */
    ChunkInfo& getStopAt()
    {
        UniqueLock lock{mutex};
        while (!stopAt) {
            Canceler canceler{};
            cond.wait(lock);
        }
        return stopAt;
    }

public:
    /**
     * Default constructs.
     */
    Impl()
        : mutex{}
        , cond{}
        , peer{}
        , startWith{}
        , stopAt{}
        , prodStore{}
    {}

    /**
     * Constructs.
     * @param[in] peer         Local peer associated with remote peer
     * @param[in] startWith    Identifies the chunk of data whose information
     *                         should be sent first
     * @param[in] prodStore    Product storage
     * @throw InvalidArgument  `startWith` is empty
     */
    Impl(   Peer&            peer,
            const ChunkInfo& startWith,
            ProdStore&       prodStore)
        : mutex{}
        , cond{}
        , peer{peer}
        , startWith{startWith}
        , stopAt{}
        , prodStore{prodStore}
    {
        if (!startWith)
            throw INVALID_ARGUMENT("Chunk-information is empty");
    }

    operator bool() const noexcept
    {
        return static_cast<bool>(startWith);
    }

    /**
     * Returns the first chunk-information to be sent.
     * @return First chunk-information to be sent
     */
    const ChunkInfo& getStart()
    {
        return startWith;
    }

    /**
     * Tells this instance that information on the given data-chunk should not
     * be sent to the remote peer.
     * @param[in] doNotSend  Chunk-information that shouldn't be sent
     * @exceptionsafety      Nothrow
     * @threadsafety         Safe
     */
    void doNotNotifyOf(const ChunkInfo& chunkInfo) noexcept
    {
        LockGuard lock{mutex};
        if (chunkInfo.isEarlierThan(stopAt) || (!stopAt && chunkInfo)) {
            stopAt = chunkInfo;
            cond.notify_one();
        }
    }

    /**
     * Returns the earliest chunk-information that shouldn't be sent to the
     * remote peer.
     * @return           Earliest chunk-information that shouldn't be sent. Will
     *                   initially be empty.
     * @exceptionsafety  Nothrow
     * @threadsafety     Safe
     * @see `doNotRequest()`
     * @see `ChunkInfo::operator bool()`
     */
    const ChunkInfo& getEarliest() const noexcept
    {
        LockGuard lock{mutex};
        return stopAt;
    }

    /**
     * Executes this instance. Returns immediately if this instance was default
     * constructed; otherwise, doesn't start until `doNotNotifyOf()` has been
     * called with a non-empty `ChunkInfo` and doesn't return until there are no
     * more notices for the remote peer in the backlog.
     * @see `doNotNotifyOf(ChunkInfo&)`
     */
    void operator()()
    {
        if (startWith) {
            static ProdIndex prevProdIndex{};
            static bool      prevProdIndexSet{false};
            getStopAt(); // Don't start until `stopAt` is set
            for (auto iter = prodStore.getChunkInfoIterator(startWith);; ++iter) {
                auto chunkInfo = *iter;
                auto prodIndex = chunkInfo.getProdIndex();
                if (prodIndex != prevProdIndex || !prevProdIndexSet) {
                    ProdInfo prodInfo{};
                    if (prodStore.getProdInfo(prodIndex, prodInfo))
                        peer.sendNotice(prodInfo);
                    prevProdIndex = prodIndex;
                    prevProdIndexSet = true;
                }
                if (!chunkInfo.isEarlierThan(getStopAt()))
                    break;
                peer.sendNotice(chunkInfo);
            }
        }
    }
};

Backlogger::Backlogger()
    : pImpl{new Impl()}
{}

Backlogger::Backlogger(
        Peer&            peer,
        const ChunkInfo& startWith,
        ProdStore&       prodStore)
    : pImpl{new Impl(peer, startWith, prodStore)}
{}

Backlogger::operator bool() const noexcept
{
    return pImpl->operator bool();
}

const ChunkInfo& Backlogger::getStart() const noexcept
{
    return pImpl->getStart();
}

void Backlogger::doNotNotifyOf(const ChunkInfo& chunkInfo) const noexcept
{
    pImpl->doNotNotifyOf(chunkInfo);
}

const ChunkInfo& Backlogger::getEarliest() const noexcept
{
    return pImpl->getEarliest();
}

void Backlogger::operator ()() {
    pImpl->operator()();
}

} // namespace
