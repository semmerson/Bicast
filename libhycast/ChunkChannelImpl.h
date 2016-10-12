/**
 * This file declares implements an object channel for chunks-of-data.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ChunkChannelImpl.h
 * @author: Steven R. Emmerson
 */

#ifndef CHUNK_CHANNEL_IMPL_H_
#define CHUNK_CHANNEL_IMPL_H_

#include "Chunk.h"
#include "Socket.h"

#include <memory>

namespace hycast {

class ChunkChannelImpl final {
    Socket   sock;
    unsigned streamId;
    unsigned version;
public:
    ChunkChannelImpl(
            Socket&        sock,
            const unsigned streamId,
            const unsigned version);
    /**
     * Returns the associated SCTP socket.
     * @returns the associated SCTP socket
     */
    Socket& getSocket() {
        return sock;
    }
    /**
     * Returns the SCTP stream ID of the current, incoming message. Waits for
     * the message if necessary.
     * @return the SCTP stream ID of the current message
     */
    unsigned getStreamId() {
        return sock.getStreamId();
    }
    /**
     * Sends a chunk-of-data.
     * @param[in] chunk  Chunk of data
     */
    void send(const ActualChunk& chunk);
    /**
     * Returns the chunk-of-data in the current message.
     * @return the chunk-of-data in the current message
     */
    std::shared_ptr<LatentChunk> recv();
    /**
     * Returns the amount of available input in bytes.
     * @return The amount of available input in bytes
     */
    size_t getSize() {
        return static_cast<size_t>(sock.getSize());
    }
};

} // namespace

#endif
