/**
 * This file implements an I/O channel.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Channel.cpp
 * @author: Steven R. Emmerson
 */

#include "Channel.h"
#include <ChannelImpl.h>

namespace hycast {

template <class T>
Channel<T>::Channel(
        Socket&            sock,
        const unsigned     streamId,
        const unsigned     version)
    : pImpl(new ChannelImpl<T>(sock, streamId, version))
{}

template <class T>
Socket& Channel<T>::getSocket() const
{
    return pImpl->getSocket();
}

template <class T>
unsigned Channel<T>::getStreamId() const
{
    return pImpl->getStreamId();
}

template <class T>
void Channel<T>::send(const Serializable& obj) const
{
    pImpl->send(obj);
}

template <class T>
std::shared_ptr<T> Channel<T>::recv()
{
    return pImpl->recv();
}

template <class T>
std::shared_ptr<ProdInfo> Channel<T>::recvProdInfo()
{
    return pImpl->recvProdInfo();
}

template <class T>
std::shared_ptr<ChunkInfo> Channel<T>::recvChunkInfo()
{
    return pImpl->recvChunkInfo();
}

template <class T>
size_t Channel<T>::getSize() const
{
    return pImpl->getSize();
}

template class Channel<ProdInfo>;
template class Channel<ChunkInfo>;

} // namespace
