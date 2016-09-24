/**
 * This file defines a chunk of data that must be read from an SCTP socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: IncomingChunk.cpp
 * @author: Steven R. Emmerson
 */

#include "arpa/inet.h"
#include "IncomingChunk.h"

namespace hycast {

IncomingChunk::IncomingChunk(
        Socket&          sock)
    : prodIndex(0),
      chunkIndex(0),
      sock(sock)
{
    if (sock.getSize() < 8)
        throw std::invalid_argument("Invalid message: msgSize=" +
                std::to_string(sock.getSize()));
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

ProdIndex IncomingChunk::getProdIndex() const
{
    return prodIndex;
}

ChunkIndex IncomingChunk::getChunkIndex() const
{
    return chunkIndex;
}

ChunkSize IncomingChunk::getSize() const
{
    return sock.getSize() - 8;
}

void IncomingChunk::drainData(void* buf)
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
