/**
 * This file implements handle classes for UDP sockets and UDP sockets.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: UdpSock.cpp
 * @author: Steven R. Emmerson
 */

#include "UdpSock.h"

#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

namespace hycast {

/**
 * Base UDP socket implementation.
 */
class UdpSock::Impl
{
protected:
    InetSockAddr localAddr; /// Address of local endpoint
    int          sd;        /// Socket descriptor

public:
    /**
     * Constructs from the Internet socket address of the local endpoint. The
     * local port number will be chosen by the system. The socket will be open
     * for receiving.
     * @param[in] localAddr   Internet socket address of local endpoint
     * @throws std::system_error  Socket couldn't be created
     */
    Impl(const InetSockAddr& localAddr = InetSockAddr())
        : localAddr{localAddr}
        , sd{localAddr.getSocket(SOCK_DGRAM)}
    {
        if (sd < 0)
            throw std::system_error(errno, std::system_category(),
                   "socket() failure");
        // Set address of local endpoint
        localAddr.bind(sd);
    }

    /**
     * Destroys.
     */
    ~Impl() noexcept
    {
        ::close(sd);
    }

    /**
     * Returns a string representation of this instance's socket.
     * @return String representation of this instance's socket
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    std::string to_string() const
    {
        return std::string("UdpSock(localAddr=") + localAddr.to_string() +
                ", sock=" + std::to_string(sd) + ")";
    }

    void shareLocalPort() const
    {
        /*
         * Allow multiple sockets to use the same port number for incoming
         * packets
         */
        const int yes = 1;
        int       status = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(yes));
        if (status)
            throw std::system_error(errno, std::system_category(),
                   "setsockopt() failure: couldn't share port-number");
    }
};

/**
 * Server UDP socket implementation.
 */
class SrvrUdpSock::Impl final : public UdpSock::Impl
{
public:
    /**
     * Constructs from the Internet socket address of the server.
     * @param[in] srvrAddr  Server's Internet socket address
     */
    Impl(const InetSockAddr& srvrAddr)
        : UdpSock::Impl(srvrAddr)
    {}

    std::string to_string() const
    {
        return std::string("SrvrUdpSock(localAddr=") + localAddr.to_string() +
                ", sock=" + std::to_string(sd) + ")";
    }
};

/**
 * Client UDP socket.
 */
class ClntUdpSock::Impl : public UdpSock::Impl
{
    InetSockAddr remoteAddr;

public:
    /**
     * Constructs from addresses for both endpoints.
     * @param[in] remoteAddr   Remote endpoint address
     * @param[in] localAddr    Local endpoint address
     */
    Impl(   const InetSockAddr& remoteAddr,
            const InetSockAddr& localAddr = InetSockAddr())
        : UdpSock::Impl{localAddr}
        , remoteAddr{remoteAddr}
    {
        // Set remote endpoint for outgoing packets
        remoteAddr.connect(sd);
    }

    std::string to_string() const
    {
        return std::string("ClntUdpSock(localAddr=") + localAddr.to_string() +
                ", sock=" + std::to_string(sd) + ")";
    }

    /**
     * Sets the hop-limit on a socket for outgoing packets.
     * @param[in] limit  Hop limit:
     *                     -         0  Restricted to same host. Won't be
     *                                  output by any interface.
     *                     -         1  Restricted to the same subnet. Won't
     *                                  be forwarded by a router (default).
     *                     -    [2,31]  Restricted to the same site,
     *                                  organization, or department.
     *                     -   [32,63]  Restricted to the same region.
     *                     -  [64,127]  Restricted to the same continent.
     *                     - [128,255]  Unrestricted in scope. Global.
     * @throws std::system_error  `setsockopt()` failure
     * @execptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void setHopLimit(
            const unsigned limit) const
    {
        remoteAddr.setHopLimit(sd, limit);
    }

    void send(
            const void*  msg,
            const size_t len) const
    {
        throw std::logic_error("Not implemented yet");
    }

    void sendv(
            struct iovec* iovec,
            const int     iovcnt) const
    {
        throw std::logic_error("Not implemented yet");
    }
};

/**
 * Multicast UDP socket. The local and remote endpoints of such a socket  have
 * the same address, which is a multicast group address. The local endpoint
 * could use the wildcard IP address instead (e.g., htonl(INADDR_ANY), but then
 * the UDP layer would pass to the socket every packet whose destination port
 * number was that of the multicast group, regardless of destination IP address.
 */
class McastUdpSock::Impl : public ClntUdpSock::Impl
{
public:
    /**
     * Constructs from the Internet socket address of the multicast group.
     * The socket will accept any packet sent to the multicast group from any
     * source.
     * @param[in] mcastAddr   Address of multicast group
     */
    Impl(const InetSockAddr& mcastAddr)
        : ClntUdpSock::Impl{mcastAddr, mcastAddr}
    {
        mcastAddr.joinMcastGroup(sd);
    }

    /**
     * Constructs a source-specific instance. The local and remote endpoints
     * will be bound to the given multicast group and only packets from the
     * source address will be accepted.
     * @param[in] mcastAddr   Address of multicast group
     * @param[in] sourceAddr  Address of source
     */
    Impl(   const InetSockAddr& mcastAddr,
            const InetAddr&     sourceAddr)
        : ClntUdpSock::Impl{mcastAddr, mcastAddr}
    {
        mcastAddr.joinSourceGroup(sd, sourceAddr);
    }

    /**
     * Returns a string representation of this instance's socket.
     * @return String representation of this instance's socket
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    std::string to_string() const
    {
        return std::string("McastUdpSock(mcastAddr=") + localAddr.to_string() +
                ", sock=" + std::to_string(sd) + ")";
    }

    /**
     * Sets whether or not a multicast packet sent to a socket will also be
     * read from the same socket. Such looping in enabled by default.
     * @param[in] enable  Whether or not to enable reception of sent packets
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    void setMcastLoop(
            const bool enable) const
    {
        localAddr.setMcastLoop(sd, enable);
    }
};

UdpSock::UdpSock(Impl* const pImpl)
    : pImpl{pImpl}
{}

std::string UdpSock::to_string() const
{
    return pImpl->to_string();
}

void UdpSock::shareLocalPort() const
{
    pImpl->shareLocalPort();
}

SrvrUdpSock::SrvrUdpSock(const InetSockAddr& srvrAddr)
    : UdpSock(new SrvrUdpSock::Impl(srvrAddr))
{}

std::string SrvrUdpSock::to_string() const
{
    return getPimpl()->to_string();
}

ClntUdpSock::ClntUdpSock(const InetSockAddr& remoteAddr)
    : UdpSock(new ClntUdpSock::Impl(remoteAddr))
{}

ClntUdpSock::ClntUdpSock(Impl* const pImpl)
    : UdpSock(pImpl)
{}

std::string ClntUdpSock::to_string() const
{
    return getPimpl()->to_string();
}

const ClntUdpSock& ClntUdpSock::setHopLimit(
        const unsigned limit) const
{
    getPimpl()->setHopLimit(limit);
    return *this;
}

void ClntUdpSock::send(
        const void*  msg,
        const size_t len) const
{
    getPimpl()->send(msg, len);
}

void ClntUdpSock::sendv(
        struct iovec* iovec,
        const int     iovcnt) const
{
    getPimpl()->sendv(iovec, iovcnt);
}

McastUdpSock::McastUdpSock(const InetSockAddr& mcastAddr)
    : ClntUdpSock(new McastUdpSock::Impl(mcastAddr))
{}

McastUdpSock::McastUdpSock(
            const InetSockAddr& mcastAddr,
            const InetAddr&     sourceAddr)
    : ClntUdpSock(new McastUdpSock::Impl(mcastAddr, sourceAddr))
{}

std::string McastUdpSock::to_string() const
{
    return getPimpl()->to_string();
}

const McastUdpSock& McastUdpSock::setMcastLoop(
        const bool enable) const
{
    getPimpl()->setMcastLoop(enable);
    return *this;
}

} // namespace
