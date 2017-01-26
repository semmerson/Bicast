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

#include <comms/ChannelImpl.h>
#include <comms/RegChannel.h>
#include <comms/VersionMsg.h>
#include "SctpSock.h"
#include "Serializable.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

namespace hycast {

template <class T>
class RegChannelImpl final : public ChannelImpl
{
public:
    /**
     * Constructs from nothing. Any attempt to use the resulting object will
     * throw an exception.
     */
    RegChannelImpl() = default;
    /**
     * Constructs from an SCTP socket, SCTP stream identifier, and protocol
     * version.
     * @param[in] sock      SCTP socket
     * @param[in] streamId  SCTP stream ID
     * @param[in] version   Protocol version
     */
    RegChannelImpl(
            SctpSock&          sock,
            const unsigned     streamId,
            const unsigned     version)
        : ChannelImpl::ChannelImpl(sock, streamId, version)
    {}

    /**
     * Sends a serializable object.
     * @param[in] obj             Serializable object.
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void send(const Serializable& obj)
    {
        const size_t                       nbytes = obj.getSerialSize(version);
        alignas(alignof(max_align_t)) char buf[nbytes];
        /*
         * The following won't throw std::invalid_argument because `nbytes` is
         * correct.
         */
        obj.serialize(buf, nbytes, version);
        sock.send(streamId, buf, nbytes);
    }

    /**
     * Returns the object in the current message.
     * @return the object in the current message
     */
    typename std::result_of<decltype(&T::deserialize)
            (const char*, size_t, unsigned)>::type recv()
    {
        size_t nbytes = getSize();
        alignas(alignof(max_align_t)) char buf[nbytes];
        sock.recv(buf, nbytes);
        return T::deserialize(buf, nbytes, version);
    }
};

template class RegChannelImpl<VersionMsg>;
template class RegChannelImpl<ProdIndex>;
template class RegChannelImpl<ProdInfo>;
template class RegChannelImpl<ChunkInfo>;

template <class T>
RegChannel<T>::RegChannel(
        SctpSock&            sock,
        const unsigned     streamId,
        const unsigned     version)
    : pImpl(new RegChannelImpl<T>(sock, streamId, version))
{
}

template <class T>
SctpSock& RegChannel<T>::getSocket() const
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
void RegChannel<T>::send(const T& obj) const
{
    pImpl->send(obj);
}

template <class T>
typename std::result_of<decltype(&T::deserialize)
        (const char*, size_t, unsigned)>::type RegChannel<T>::recv()
{
    return pImpl->recv();
}

template class RegChannel<VersionMsg>;
template class RegChannel<ProdInfo>;
template class RegChannel<ChunkInfo>;
template class RegChannel<ProdIndex>;

} // namespace
