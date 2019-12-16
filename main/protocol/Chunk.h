/**
 * Fundamental types that are exchanged on a network.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Chunk.h
 *  Created on: Nov 22, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PROTOCOL_CHUNK_H_
#define MAIN_PROTOCOL_CHUNK_H_

#include <memory>

namespace hycast {

class PeerProto;

/**
 * Interface for chunks that are sent to a remote peer.
 */
class OutChunk
{
public:
    virtual ~OutChunk() noexcept;

    operator bool() const =0;

    virtual void send(PeerProto& proto) const =0;
};

} // namespace

#endif /* MAIN_PROTOCOL_CHUNK_H_ */
