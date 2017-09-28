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

#include "Backlogger.h"

namespace hycast {

class Backlogger::Impl
{
    Peer      peer;
    ChunkInfo startWith;
    ChunkInfo earliest;

public:
    /**
     * Default constructs.
     */
    Impl()
        : peer{}
        , startWith{}
        , earliest{}
    {}

    /**
     * Constructs.
     * @param[in] peer         Local peer associated with remote peer
     * @param[in] startWith    Identifies the chunk of data whose information
     *                         should be sent first
     * @throw InvalidArgument  `startWith` is empty
     */
    Impl(   Peer& peer,
            const ChunkInfo& startWith)
        : peer{peer}
        , startWith{startWith}
        , earliest{}
    {
        if (!startWith)
            throw INVALID_ARGUMENT("Chunk-information is empty");
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
     * @threadsafety         Compatible but not safe
     */
    void doNotSend(const ChunkInfo& chunkInfo) noexcept
    {
        if (!earliest || chunkInfo.isEarlierThan(earliest))
            earliest = chunkInfo;
    }

    /**
     * Returns the earliest chunk-information that shouldn't be sent to the
     * remote peer.
     * @return           Earliest chunk-information that shouldn't be sent. Will
     *                   initially be empty.
     * @exceptionsafety  Nothrow
     * @threadsafety     Compatible but not safe
     * @see `doNotRequest()`
     * @see `ChunkInfo::operator bool()`
     */
    const ChunkInfo& getEarliest() const noexcept
    {
        return earliest;
    }
};

Backlogger::Backlogger()
    : pImpl{new Impl()}
{}

Backlogger::Backlogger(
        Peer&            peer,
        const ChunkInfo& startWith)
    : pImpl{new Impl(peer, startWith)}
{}

const ChunkInfo& Backlogger::getStart() const noexcept
{
    return pImpl->getStart();
}

void Backlogger::doNotSend(const ChunkInfo& chunkInfo) noexcept
{
    pImpl->doNotSend(chunkInfo);
}

const ChunkInfo& Backlogger::getEarliest() const noexcept
{
    return pImpl->getEarliest();
}

} // namespace
