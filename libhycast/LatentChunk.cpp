/**
 * This file defines a chunk of data that is not yet realized.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: LatentChunk.cpp
 * @author: Steven R. Emmerson
 */

#include <arpa/inet.h>
#include <Chunk.h>

namespace hycast {

LatentChunk::LatentChunk(
        Channel& channel)
    : info(),
      channel(channel)
{
    if (channel.getSize() < 8)
        throw std::invalid_argument("Too little input: " +
                std::to_string(channel.getSize()) + " bytes");
    uint32_t pi;
    uint32_t ci;
    struct iovec iov[2];
    iov[0].iov_base = &pi;
    iov[0].iov_len = sizeof(pi);
    iov[1].iov_base = &ci;
    iov[1].iov_len = sizeof(ci);
    sock.recvv(iov, 2, MSG_PEEK);
    prodIndex = ntohl(pi);
    chunkIndex = ntohl(ci);
    if (prodIndex > prodIndexMax || chunkIndex > chunkIndexMax)
        throw std::invalid_argument("Invalid message: prodIndex=" +
                std::to_string(prodIndex) + ", chunkIndex=" +
                std::to_string(chunkIndex));
}

ProdIndex LatentChunk::getProdIndex() const
{
    return prodIndex;
}

ChunkIndex LatentChunk::getChunkIndex() const
{
    return chunkIndex;
}

ChunkSize LatentChunk::getSize() const
{
    return sock.getSize() - 8;
}

void LatentChunk::drainData(void* buf)
{
    uint8_t tmp[8];
    struct iovec iov[2];
    iov[0].iov_base = &tmp;
    iov[0].iov_len = 8;
    iov[1].iov_base = buf;
    iov[1].iov_len = sock.getSize();
    sock.recvv(iov, 2, 0);
}

} // namespace
