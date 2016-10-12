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
std::shared_ptr<T> ChannelImpl<T>::recv()
{
    size_t nbytes = getSize();
    alignas(alignof(max_align_t)) uint8_t buf[nbytes];
    sock.recv(buf, nbytes);
    return std::shared_ptr<T>(new T(buf, nbytes, version));
}

template class ChannelImpl<ProdInfo>;
template class ChannelImpl<ChunkInfo>;

}
