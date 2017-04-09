/**
 * This file declares an interface for an I/O channel.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Channel.h
 * @author: Steven R. Emmerson
 */

#ifndef CHUNK_CHANNEL_H_
#define CHUNK_CHANNEL_H_

#include "Chunk.h"
#include <memory>
#include <cstddef>

#include "../net/SctpSock.h"
#include "../p2p/Channel.h"

namespace hycast {

class ChannelImpl; // Forward declaration of implementation
class ChunkChannelImpl; // Forward declaration of implementation

class ChunkChannel final : public Channel {
    std::shared_ptr<ChunkChannelImpl> pImpl;
public:
    /**
     * Constructs from nothing. Any attempt to use the resulting instance will
     * throw an exception.
     */
    ChunkChannel() =default;
    /**
     * Constructs from an SCTP socket, a stream identifier, and a protocol
     * version.
     * @param[in] sock      SCTP Socket
     * @param[in] streamId  Stream identifier
     * @param[in] version   Protocol version
     */
    ChunkChannel(
            SctpSock&            sock,
            const unsigned     streamId,
            const unsigned     version);
    /**
     * Returns the associated SCTP socket.
     * @returns the associated SCTP socket
     */
    SctpSock& getSocket() const;
    /**
     * Returns the SCTP stream ID of the current message. Waits for the message
     * if necessary. The message is left in the input buffer.
     * @return the SCTP stream ID of the current message
     */
    unsigned getStreamId() const;
    /**
     * Returns the size of the current message in bytes.
     * @return The size of the current message in bytes
     */
    size_t getSize() const;
    /**
     * Sends a chunk-of-data.
     * @param[in] chunk  Chunk of data
     */
    void send(const ActualChunk& chunk) const;
    /**
     * Returns the chunk-of-data in the current message.
     * @return the chunk-of-data in the current message
     */
    LatentChunk recv();
};

} // namespace

#endif /* CHUNK_CHANNEL_H_ */
