/**
 * This file defines the implementation of an I/O channel for exchanging
 * serializable objects.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: RegChannelImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "RegChannelImpl.h"
#include "VersionMsg.h"

#include <cstddef>
#include <memory>

namespace hycast {

template <class T>
RegChannelImpl<T>::RegChannelImpl(
        Socket&            sock,
        const unsigned     streamId,
        const unsigned     version)
    : ChannelImpl::ChannelImpl(sock, streamId, version)
{
}

template <class T>
void RegChannelImpl<T>::send(const Serializable& obj)
{
    const size_t nbytes = obj.getSerialSize(version);
    alignas(alignof(max_align_t)) char buf[nbytes];
    obj.serialize(buf, nbytes, version);
    sock.send(streamId, buf, nbytes);
}

template <class T>
typename std::result_of<decltype(&T::deserialize)
        (const char*, size_t, unsigned)>::type RegChannelImpl<T>::recv()
{
    size_t nbytes = getSize();
    alignas(alignof(max_align_t)) char buf[nbytes];
    sock.recv(buf, nbytes);
    return T::deserialize(buf, nbytes, version);
}

template class RegChannelImpl<VersionMsg>;
template class RegChannelImpl<ProdIndex>;
template class RegChannelImpl<ProdInfo>;
template class RegChannelImpl<ChunkInfo>;

} // namespace
