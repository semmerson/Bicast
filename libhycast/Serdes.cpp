/**
 * This file defines a serializer/deserializer.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Serdes.cpp
 * @author: Steven R. Emmerson
 */

#include "Serdes.h"

#include <arpa/inet.h>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <sys/uio.h>

namespace hycast {

void Serdes::serialize(
            Socket&         sock,
            const unsigned  streamId,
            const ProdInfo& prodInfo)
{
    const std::string&            name{prodInfo.getName()};
    auto                          nameLen = name.size();
    struct iovec                  iovec[PROD_INFO_IOVCNT];
    decltype(ProdInfo::index)     index = htonl(prodInfo.index);
    decltype(ProdInfo::size)      size = htonl(prodInfo.size);
    decltype(ProdInfo::chunkSize) chunkSize = htons(prodInfo.chunkSize);
    iovec[0].iov_base = &index;
    iovec[0].iov_len = sizeof(index);
    iovec[1].iov_base = &size;
    iovec[1].iov_len = sizeof(size);
    iovec[2].iov_base = &chunkSize;
    iovec[2].iov_len = sizeof(chunkSize);
    iovec[3].iov_base = name.data();
    iovec[3].iov_len = nameLen;
    sock.send(streamId, iovec, PROD_INFO_IOVCNT);
}

void Serdes::deserialize(
        const ProdInfo& prodInfo,
        const size_t    nbytes)
{
    if (nbytes < PROD_INFO_FIXED_LEN)
        throw std::invalid_argument("Serialized product-info is too small: " +
                std::to_string(nbytes) + " bytes");
    const size_t nameLen = nbytes - PROD_INFO_FIXED_LEN;
    struct iovec iovec[PROD_INFO_IOVCNT];
    iovec[0].iov_base = &prodInfo.index;
    iovec[0].iov_len = sizeof(prodInfo.index);
    iovec[1].iov_base = &prodInfo.size;
    iovec[1].iov_len = sizeof(prodInfo.size);
    iovec[2].iov_base = &prodInfo.chunkSize;
    iovec[2].iov_len = sizeof(prodInfo.chunkSize);
    prodInfo.name.reserve(nameLen);
    iovec[3].iov_base = prodInfo.name.data();
    iovec[3].iov_len = nameLen;
    conn.recvProdInfo(prodInfoIoVec.iovec, PROD_INFO_IOVCNT);
    prodInfo.index = ntohl(prodInfo.index);
    prodInfo.size = ntohl(prodInfo.size);
    prodInfo.chunkSize = ntohs(prodInfo.chunkSize);
    prodInfo.name.append("");
}

} // namespace
