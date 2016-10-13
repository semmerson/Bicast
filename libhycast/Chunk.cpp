/**
 * This file implements two types of chunks of data: 1) a latent chunk that must
 * be read from an object channel; and 2) a reified chunk with a pointer to its
 * data. The two types are in the same file to support keeping their
 * serialization and de-serialization methods consistent.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Chunk.cpp
 * @author: Steven R. Emmerson
 */

#include "Chunk.h"

#include <cstddef>
#include <cstdint>

namespace hycast {

LatentChunk::LatentChunk(
        Socket&        sock,
        const unsigned version)
    : info(),
      sock(sock),
      size(0),
      version(version)
{
    // Keep consistent with ActualChunk::serialize()
    unsigned nbytes = info.getSerialSize(version);
    alignas(alignof(max_align_t)) uint8_t buf[nbytes];
    sock.recv(buf, nbytes, MSG_PEEK);
    info = ChunkInfo(buf, nbytes, version);
    size = sock.getSize() - nbytes;
}

void LatentChunk::drainData(void* data)
{
    unsigned nbytes = info.getSerialSize(version);
    alignas(alignof(max_align_t)) uint8_t buf[nbytes];
    struct iovec iovec[2];
    iovec[0].iov_base = buf;
    iovec[0].iov_len = nbytes;
    iovec[1].iov_base = const_cast<void*>(data);
    iovec[1].iov_len = size;
    sock.recvv(iovec, 2);
}

void ActualChunk::serialize(
        Socket&        sock,
        const unsigned streamId,
        const unsigned version) const
{
    // Keep consistent with LatentChunk::LatentChunk()
    unsigned nbytes = info.getSerialSize(version);
    alignas(alignof(max_align_t)) uint8_t buf[nbytes];
    info.serialize(buf, nbytes, version);
    struct iovec iovec[2];
    iovec[0].iov_base = buf;
    iovec[0].iov_len = nbytes;
    iovec[1].iov_base = const_cast<void*>(data);
    iovec[1].iov_len = size;
    sock.sendv(streamId, iovec, 2);
}

} // namespace
