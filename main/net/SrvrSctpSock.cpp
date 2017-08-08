/**
 * This file implements a server-side SCTP socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ServerSocket.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "SrvrSctpSock.h"

#include <unistd.h>

namespace hycast {

class SrvrSctpSock::Impl
{
    int srvrSd;
    int numStreams;

public:
    Impl()
        : srvrSd{-1}
        , numStreams{0}
    {}

    Impl(   const InetSockAddr& addr,
            const int           numStreams,
            const int           queueSize)
        : srvrSd{-1}
        , numStreams{numStreams}
    {
        try {
            if (numStreams <= 0)
                throw InvalidArgument(__FILE__, __LINE__,
                        "Invalid number of SCTP streams: " +
                        std::to_string(numStreams));
            if (queueSize <= 0)
                throw InvalidArgument(__FILE__, __LINE__,
                        "Invalid length for ::accept() queue: " +
                        std::to_string(queueSize));
            srvrSd = SctpSock::createSocket();
            try {
                addr.bind(srvrSd);
                if (::listen(srvrSd, queueSize))
                    throw SystemError(__FILE__, __LINE__,
                            "listen() failure: sock=" + std::to_string(srvrSd) +
                            ", addr=" + addr.to_string());
            }
            catch (const std::exception& e) {
                (void)::close(srvrSd);
                throw;
            }
        }
        catch (const std::exception& e) {
            std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                    "Couldn't construct server-side SCTP socket"));
        }
    }

    ~Impl() noexcept
    {
        if (srvrSd >= 0)
            ::close(srvrSd);
    }

    int getSock() const noexcept
    {
        return srvrSd;
    }

    int getNumStreams() const noexcept
    {
        return numStreams;
    }

    SctpSock accept() const
    {
        socklen_t len = 0;
        const int sd = ::accept(srvrSd, (struct sockaddr*)nullptr, &len);
        if (sd < 0)
            throw SystemError(__FILE__, __LINE__,
                    "accept() failure: srvrSd=" + std::to_string(srvrSd));
        try {
            return SctpSock{sd, numStreams};
        }
        catch (const std::exception& e) {
            (void)::close(sd);
            throw;
        }
    }
};

/******************************************************************************/

SrvrSctpSock::SrvrSctpSock()
    : pImpl{new Impl()}
{}

SrvrSctpSock::SrvrSctpSock(
        const InetSockAddr& addr,
        const int           numStreams,
        const int           queueSize)
    : pImpl{new Impl(addr, numStreams, queueSize)}
{}

SrvrSctpSock& SrvrSctpSock::operator =(const SrvrSctpSock& rhs)
{
    if (pImpl.get() != rhs.pImpl.get())
        pImpl = rhs.pImpl;
    return *this;
}

bool SrvrSctpSock::operator ==(const SrvrSctpSock& that) const noexcept
{
    return pImpl.get() == that.pImpl.get();
}

int SrvrSctpSock::getSock() const noexcept
{
    return pImpl->getSock();
}

int SrvrSctpSock::getNumStreams() const noexcept
{
    return pImpl->getNumStreams();
}

SctpSock SrvrSctpSock::accept() const
{
    return pImpl->accept();
}

std::string SrvrSctpSock::to_string() const
{
    return "SrvrSctpSock{sd: " + std::to_string(pImpl->getSock()) +
            ", numStreams: " + std::to_string(pImpl->getNumStreams()) + "}";
}

} // namespace
