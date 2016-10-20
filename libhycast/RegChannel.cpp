/**
 * This file implements an I/O channel for `Serializable` objects.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: RegChannel.cpp
 * @author: Steven R. Emmerson
 */

#include "ChannelImpl.h"
#include "RegChannel.h"
#include <RegChannelImpl.h>

#include <memory>

namespace hycast {

template <class T>
RegChannel<T>::RegChannel(
        Socket&            sock,
        const unsigned     streamId,
        const unsigned     version)
    : pImpl(new RegChannelImpl<T>(sock, streamId, version))
{
}

template <class T>
Socket& RegChannel<T>::getSocket() const
{
    return pImpl->getSocket();
}

template <class T>
unsigned RegChannel<T>::getStreamId() const
{
    return pImpl->getStreamId();
}

template <class T>
size_t RegChannel<T>::getSize() const
{
    return pImpl->getSize();
}

template <class T>
void RegChannel<T>::send(const Serializable& obj) const
{
    pImpl->send(obj);
}

template <class T>
typename std::result_of<decltype(&T::deserialize)
        (const char*, size_t, unsigned)>::type RegChannel<T>::recv()
{
    return pImpl->recv();
}

template class RegChannel<ProdInfo>;
template class RegChannel<ChunkInfo>;
template class RegChannel<ProdIndex>;

} // namespace
