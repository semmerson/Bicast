/**
 * Interface for sending to a remote peer.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: SendPeer.h
 *  Created on: May 10, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PEER_PEERMSGSNDR_H_
#define MAIN_PEER_PEERMSGSNDR_H_

#include <main/protocol/Chunk.h>
#include <memory>

namespace hycast {

class PeerMsgSndr
{
public:
    virtual ~PeerMsgSndr() noexcept
    {}

    /**
     * Notifies the remote peer about the availability of a `Chunk`.
     *
     * @param[in] notice   ID of available `Chunk`
     * @retval    `true`   Notice was sent
     * @retval    `false`  Notice was not set
     */
    virtual bool unbufNotify(const ChunkId& notice) =0;
};

} // namespace

#endif /* MAIN_PEER_PEERMSGSNDR_H_ */
