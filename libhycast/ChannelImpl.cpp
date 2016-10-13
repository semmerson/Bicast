/**
 * This file implements an I/O channel for sending and receiving Hycast
 * messages.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ChannelImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "ChannelImpl.h"

#include <cstddef>
#include <memory>

namespace hycast {

template <class T>
ChannelImpl<T>::ChannelImpl(
        Socket&            sock,
        const unsigned     streamId,
        const unsigned     version)
    : sock(sock),
      streamId(streamId),
      version(version)
{
}

template <class T>
void ChannelImpl<T>::send(const Serializable& obj)
{
    size_t nbytes = obj.getSerialSize(version);
    alignas(alignof(max_align_t)) uint8_t buf[nbytes];
    obj.serialize(buf, nbytes, version);
    sock.send(streamId, buf, nbytes);
}

template <class T>
typename std::result_of<decltype(&T::deserialize)
        (const void*, size_t, unsigned)>::type ChannelImpl<T>::recv()
{
    size_t nbytes = getSize();
    alignas(alignof(max_align_t)) uint8_t buf[nbytes];
    sock.recv(buf, nbytes);
    return T::deserialize(buf, nbytes, version);
}

template class ChannelImpl<ProdInfo>;
template class ChannelImpl<ChunkInfo>;

}
