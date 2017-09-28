/**
 * This file declares a class that retrieves the backlog of data-products when
 * a peer first connects to its remote peer.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Backlogger.h
 *  Created on: Sep 21, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_COMMS_P2P_BACKLOGGER_H_
#define MAIN_COMMS_P2P_BACKLOGGER_H_

#include "ChunkInfo.h"
#include "Peer.h"

#include <memory>

namespace hycast {

class Backlogger
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs.
     */
    Backlogger();

    /**
     * Constructs.
     * @param[in] peer         Local peer associated with remote peer
     * @param[in] startWith    Identifies the chunk of data whose information
     *                         should be sent first
     * @throw InvalidArgument  `startWith` is empty
     */
    Backlogger(
            Peer&            peer,
            const ChunkInfo& startWith);

    /**
     * Returns the first chunk-information to be sent.
     * @return First chunk-information to be sent
     */
    const ChunkInfo& getStart() const noexcept;

    /**
     * Tells this instance that information on the given data-chunk should not
     * be sent to the remote peer.
     * @param[in] doNotSend  Chunk-information that shouldn't be sent
     * @exceptionsafety      Nothrow
     * @threadsafety         Compatible but not safe
     */
    void doNotSend(const ChunkInfo& doNotSend) noexcept;

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
    const ChunkInfo& getEarliest() const noexcept;
};

} // namespace

#endif /* MAIN_COMMS_BACKLOGGER_H_ */
