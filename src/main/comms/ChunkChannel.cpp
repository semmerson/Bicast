/**
 * This file implements an object channel for chunks-of-data.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ChunkChannel.cpp
 * @author: Steven R. Emmerson
 */

#include <comms/ChannelImpl.h>
#include <comms/ChunkChannel.h>
#include "Chunk.h"
#include "SctpSock.h"

namespace hycast {

class ChunkChannelImpl final : public ChannelImpl
{
public:
    /**
     * Constructs from nothing. Any attempt to use the resulting instance will
     * throw an exception.
     */
    ChunkChannelImpl() =default;

    /**
     * Constructs from an SCTP socket, SCTP stream identifier, and protocol
     * version.
     * @param[in] sock      SCTP socket
     * @param[in] streamId  SCTP stream ID
     * @param[in] version   Protocol version
     */
    ChunkChannelImpl(
            SctpSock&        sock,
            const unsigned streamId,
            const unsigned version)
        : ChannelImpl::ChannelImpl(sock, streamId, version)
    {}

    /**
     * Sends a chunk-of-data.
     * @param[in] chunk  Chunk of data
     */
    void send(const ActualChunk& chunk)
    {
        chunk.serialize(sock, streamId, version);
    }

    /**
     * Returns the chunk-of-data in the current message.
     * @return the chunk-of-data in the current message
     */
    LatentChunk recv()
    {
        return LatentChunk(sock, version);
    }
};

ChunkChannel::ChunkChannel(
        SctpSock&      sock,
        const unsigned streamId,
        const unsigned version)
    : pImpl(new ChunkChannelImpl(sock, streamId, version))
{
}

SctpSock& ChunkChannel::getSocket() const
{
    return pImpl->getSocket();
}

unsigned ChunkChannel::getStreamId() const
{
    return pImpl->getStreamId();
}

size_t ChunkChannel::getSize() const
{
    return pImpl->getSize();
}

void ChunkChannel::send(const ActualChunk& chunk) const
{
    pImpl->send(chunk);
}

LatentChunk ChunkChannel::recv()
{
    return pImpl->recv();
}

} // namespace
