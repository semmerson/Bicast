/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Socket.cpp
 * @author: Steven R. Emmerson
 *
 * This file defines a socket.
 */

#include "Socket.h"
#include "SocketImpl.h"

#include <errno.h>
#include <netinet/sctp.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

namespace hycast {

Socket::Socket()
    : pImpl(new SocketImpl())
{}

Socket::Socket(
        const int      sd,
        const uint16_t numStreams)
    : pImpl(new SocketImpl(sd, numStreams))
{}

Socket::Socket(SocketImpl* impl)
    : pImpl(impl)
{}

Socket::Socket(std::shared_ptr<SocketImpl> sptr)
    : pImpl(sptr)
{}

uint16_t Socket::getNumStreams() const
{
    return pImpl->getNumStreams();
}

bool Socket::operator ==(const Socket& that) const noexcept
{
    return *pImpl.get() == *that.pImpl.get();
}

unsigned Socket::getStreamId() const
{
    return pImpl->getStreamId();
}

uint32_t Socket::getSize() const
{
    return pImpl->getSize();
}

std::string Socket::to_string() const
{
    return pImpl->to_string();
}

void Socket::send(
        const unsigned streamId,
        const void*    msg,
        const size_t   len) const
{
    pImpl->send(streamId, msg, len);
}

void Socket::sendv(
        const unsigned streamId,
        struct iovec*  iovec,
        const int      iovcnt) const
{
    pImpl->sendv(streamId, iovec, iovcnt);
}

void Socket::recv(
        void*        msg,
        const size_t len,
        const int    flags) const
{
    pImpl->recv(msg, len, flags);
}

void Socket::recvv(
        struct iovec* iovec,
        const int     iovcnt,
        const int     flags) const
{
    pImpl->recvv(iovec, iovcnt, flags);
}

bool Socket::hasMessage() const
{
    return pImpl->hasMessage();
}

void Socket::discard() const
{
    pImpl->discard();
}

void Socket::close() const
{
    pImpl->close();
}

} // namespace
