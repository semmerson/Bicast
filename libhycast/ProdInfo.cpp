/**
 * This file defines information on a product.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdInfo.cpp
 * @author: Steven R. Emmerson
 */

#include "ProdInfo.h"

#include <arpa/inet.h>
#include <stdexcept>

namespace hycast {

ProdInfo::ProdInfo()
    : name(""),
      index(0),
      size(0),
      chunkSize(0)
{
}

ProdInfo::ProdInfo(
        const std::string& name,
        const uint32_t     index,
        const uint32_t     size,
        const uint16_t     chunkSize)
    : name(name),
      index(index),
      size(size),
      chunkSize(chunkSize)
{
}

ProdInfo::ProdInfo(
        Socket&        sock,
        const unsigned version)
    : name(),
      index(0),
      size(0),
      chunkSize(0)
{
    static const size_t FIXED_LEN = sizeof(index) + sizeof(size) +
            sizeof(chunkSize);
    size_t nbytes = sock.getSize();
    if (nbytes < FIXED_LEN)
        throw std::underflow_error("Serialized product-info less than " +
                std::to_string(FIXED_LEN) + " bytes: size=" +
                std::to_string(nbytes));
    const size_t nameLen = nbytes - FIXED_LEN;
    struct iovec iovec[IOVCNT];
    iovec[0].iov_base = &index;
    iovec[0].iov_len = sizeof(index);
    iovec[1].iov_base = &size;
    iovec[1].iov_len = sizeof(size);
    iovec[2].iov_base = &chunkSize;
    iovec[2].iov_len = sizeof(chunkSize);
    char nam[nameLen];
    iovec[3].iov_base = nam;
    iovec[3].iov_len = nameLen;
    sock.recvv(iovec, IOVCNT, 0);
    index = ntohl(index);
    size = ntohl(size);
    chunkSize = ntohs(chunkSize);
    name.assign(nam, nameLen);
}

void ProdInfo::serialize(
        Socket&        sock,
        const unsigned streamId,
        const unsigned version) const
{
    struct iovec                  iovec[IOVCNT];
    decltype(ProdInfo::index)     netIndex = htonl(index);
    decltype(ProdInfo::size)      netSize = htonl(size);
    decltype(ProdInfo::chunkSize) netChunkSize = htons(chunkSize);
    iovec[0].iov_base = &netIndex;
    iovec[0].iov_len = sizeof(netIndex);
    iovec[1].iov_base = &netSize;
    iovec[1].iov_len = sizeof(netSize);
    iovec[2].iov_base = &netChunkSize;
    iovec[2].iov_len = sizeof(netChunkSize);
    // The following cast is safe since the data is only being read
    iovec[3].iov_base = const_cast<char*>(name.data());
    iovec[3].iov_len = name.size();
    sock.sendv(streamId, iovec, IOVCNT);
}

} // namespace
