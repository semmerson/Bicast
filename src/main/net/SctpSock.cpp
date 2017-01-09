/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Socket.cpp
 * @author: Steven R. Emmerson
 *
 * This file defines an SCTP socket.
 */

#include "SctpSockImpl.h"

namespace hycast {

SctpSock::SctpSock()
    : pImpl(new SctpSockImpl())
{}

SctpSock::SctpSock(
        const int      sd,
        const uint16_t numStreams)
    : pImpl(new SctpSockImpl(sd, numStreams))
{}

SctpSock::SctpSock(SctpSockImpl* impl)
    : pImpl(impl)
{}

SctpSock::SctpSock(std::shared_ptr<SctpSockImpl> sptr)
    : pImpl(sptr)
{}

uint16_t SctpSock::getNumStreams() const
{
    return pImpl->getNumStreams();
}

const InetSockAddr& SctpSock::getRemoteAddr()
{
    return pImpl->getRemoteAddr();
}

bool SctpSock::operator ==(const SctpSock& that) const noexcept
{
    return *pImpl.get() == *that.pImpl.get();
}

unsigned SctpSock::getStreamId() const
{
    return pImpl->getStreamId();
}

uint32_t SctpSock::getSize() const
{
    return pImpl->getSize();
}

std::string SctpSock::to_string() const
{
    return pImpl->to_string();
}

void SctpSock::send(
        const unsigned streamId,
        const void*    msg,
        const size_t   len) const
{
    pImpl->send(streamId, msg, len);
}

void SctpSock::sendv(
        const unsigned streamId,
        struct iovec*  iovec,
        const int      iovcnt) const
{
    pImpl->sendv(streamId, iovec, iovcnt);
}

void SctpSock::recv(
        void*        msg,
        const size_t len,
        const int    flags) const
{
    pImpl->recv(msg, len, flags);
}

void SctpSock::recvv(
        struct iovec* iovec,
        const int     iovcnt,
        const int     flags) const
{
    pImpl->recvv(iovec, iovcnt, flags);
}

bool SctpSock::hasMessage() const
{
    return pImpl->hasMessage();
}

void SctpSock::discard() const
{
    pImpl->discard();
}

void SctpSock::close() const
{
    pImpl->close();
}

} // namespace
