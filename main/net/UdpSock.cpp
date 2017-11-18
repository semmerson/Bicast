/**
 * This file implements UDP sockets.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: UdpSock.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "Thread.h"
#include "UdpSock.h"

#include <cerrno>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <system_error>
#include <unistd.h>

namespace hycast {

/**
 * Abstract base UDP socket implementation.
 */
class UdpSock::Impl
{
protected:
    int sd; /// Socket descriptor

    /**
     * Constructs from the Internet socket address of an endpoint.
     * @param[in] inetSockAddr   Internet socket address of an endpoint
     * @throws std::system_error  Socket couldn't be created
     */
    explicit Impl(const InetSockAddr& inetSockAddr)
        : sd{inetSockAddr.getSocket(SOCK_DGRAM)}
    {}

public:
    /**
     * Default constructs.
    Impl()
        : sd{-1}
    {}
     */

    Impl(const Impl& impl) =delete;
    Impl(const Impl&& impl) =delete;

    /**
     * Destroys. Closes the underlying socket.
     */
    virtual ~Impl() noexcept
    {
        ::close(sd);
        //std::cerr << "UdpSock::~UdpSock(): Closed socket " << sd << '\n';
    }

    Impl& operator=(const Impl& impl) =delete;
    Impl& operator=(const Impl&& impl) =delete;

    /**
     * Returns a string representation of this instance's socket.
     * @return String representation of this instance's socket
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    virtual std::string to_string() const =0;
};

/**
 * Input UDP socket implementation.
 */
class InUdpSock::Impl : public UdpSock::Impl
{
    UdpPayloadSize currRecSize;

    void init()
    {
        currRecSize = 0;
    }

    /**
     * Allows multiple sockets to use the same port number for incoming packets
     * @throws std::system_error  setsockopt() failure
     */
    void shareLocalPort() const
    {
        const int yes = 1;
        int       status = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(yes));
        if (status)
            throw SYSTEM_ERROR(
                    "Couldn't share local port number for incoming packets: "
                    "sock=" + std::to_string(sd));
    }

    void checkReadStatus(const ssize_t nbytes)
    {
        if (nbytes < 0)
            throw SYSTEM_ERROR(std::string{"recvmsg() failure: sock="} +
                    std::to_string(sd));
    }

protected:
    InetSockAddr localSockAddr;    /// Internet socket address of local endpoint

    void bind()
    {
        localSockAddr.bind(sd);
    }

public:
    /**
     * Default constructs.
    Impl()
        : UdpSock::Impl()
        , localSockAddr{}
    {
        init();
    }
     */

    /**
     * Constructs from the Internet socket address of the local endpoint.
     * @param[in] localSockAddr  Internet socket address of local endpoint
     * @param[in] sharePort      Whether or not the local port number should be
     *                           shared amongst multiple sockets
     */
    explicit Impl(const InetSockAddr& localSockAddr, bool sharePort)
        : UdpSock::Impl{localSockAddr}
        , localSockAddr{localSockAddr}
    {
    	try {
            init();
            if (sharePort)
                shareLocalPort(); // Must be before `bind()`
            bind(); // Set address of local endpoint
    	}
    	catch (const std::exception& e) {
            std::throw_with_nested(RUNTIME_ERROR(
                    "Couldn't construct input UDP socket"));
    	}
    }

    /**
     * Destroys.
     */
    virtual ~Impl() noexcept =default;

    virtual std::string to_string() const
    {
        return std::string("InUdpSock(addr=") + localSockAddr.to_string() +
                ", sock=" + std::to_string(sd) + ")";
    }

    /**
     * Returns the local socket address.
     * @return The local socket address
     * @exceptionsafety  Noexcept
     * @threadsafety     Safe
     */
    const InetSockAddr getLocalAddr() const noexcept
    {
        return localSockAddr;
    }

    /**
     * Scatter-receives a datagram. Waits for the datagram if necessary. If the
     * requested number of bytes to be read is less than the datagram size, then
     * the excess bytes are discarded.
     * @param[in] iovec     Scatter-read vector
     * @param[in] iovcnt    Number of elements in scatter-read vector
     * @param[in] peek      Whether or not to peek at the datagram. The data is
     *                      treated as unread and the next recv() or similar
     *                      function shall still return this data.
     * @retval    0         Socket is closed
     * @return              Actual number of bytes read into the buffers.
     * @throws SystemError  I/O error reading from socket
     */
    UdpPayloadSize recv(
           const struct iovec* iovec,
           const int           iovcnt,
           const bool          peek = false)
    {
        struct msghdr msghdr = {};
        msghdr.msg_iov = const_cast<struct iovec*>(iovec);
        msghdr.msg_iovlen = iovcnt;
        ssize_t nbytes;
        {
            Canceler canceler{};
            nbytes = ::recvmsg(sd, &msghdr, peek ? MSG_PEEK : 0);
        }
#if 0
        ::printf("UdpSock::recv(): iovcnt=%d, iovec[0].iov_len=%zu, "
                "peek=%d, nbytes=%zd\n", iovcnt, iovec[0].iov_len, peek,
                nbytes);
#endif
        checkReadStatus(nbytes);
        currRecSize = peek ? nbytes : 0;
        return nbytes;
    }

    /**
     * Receives a datagram. Waits for the datagram if necessary. If the requested
     * number of bytes to be read is less than the datagram size, then the excess
     * bytes are discarded.
     * @param[in] buf       Receive buffer
     * @param[in] len       Size of receive buffer in bytes
     * @param[in] peek      Whether or not to peek at the datagram. The data is
     *                      treated as unread and the next recv() or similar
     *                      function shall still return this data.
     * @retval    0         Socket is closed
     * @return              Actual number of bytes read into the buffer.
     * @throws SystemError  I/O error reading from socket */
    size_t recv(
           void* const  buf,
           const size_t len,
           const bool   peek = false)
    {
        struct iovec iov;
        iov.iov_base = buf;
        iov.iov_len = len;
        return recv(&iov, 1, peek);
    }

    /**
     * Discards the current datagram. Does nothing if there is no current
     * datagram. Idempotent.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Thread-compatible but not thread-safe
     */
    void discard()
    {
        if (currRecSize) {
            char         buf;
            recv(&buf, sizeof(buf));
            currRecSize = 0;
        }
    }

    /**
     * Indicates if there's a current datagram.
     */
    bool hasRecord()
    {
        return currRecSize != 0;
    }

    UdpPayloadSize getSize() const noexcept
    {
        return currRecSize;
    }
};

/**
 * Output UDP socket.
 */
class OutUdpSock::Impl final : public UdpSock::Impl
{
    struct sockaddr_storage sockAddrStorage;

protected:
    InetSockAddr remoteSockAddr;

public:
    /**
     * Constructs from Internet address of remote endpoint.
     * @param[in] remoteSockAddr   Remote endpoint address
     * @throws std::system_error  `socket()` failure
     */
    explicit Impl(
            const InetSockAddr& remoteSockAddr)
        : UdpSock::Impl{remoteSockAddr}
        , remoteSockAddr{remoteSockAddr}
    {
    	try {
            remoteSockAddr.setSockAddrStorage(sd, sockAddrStorage);
            /*
             * Sets address of remote endpoint for function `::send()` (but not
             * `::sendmsg()`)
             */
            remoteSockAddr.connect(sd);
    	}
        catch (const std::exception& e) {
            std::throw_with_nested(RUNTIME_ERROR(
                    "Couldn't create outgoing UDP socket"));
        }
    }

    /**
     * Destroys.
     */
    virtual ~Impl() noexcept =default;

    virtual std::string to_string() const
    {
        return std::string("OutUdpSock(addr=") + remoteSockAddr.to_string() +
                ", sock=" + std::to_string(sd) + ")";
    }

    /**
     * Sets the interface to use for outgoing datagrams.
     * @param[in] inetAddr  Internet address of interface
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void setInterface(const InetAddr& inetAddr)
    {
        inetAddr.setInterface(sd);
    }

    /**
     * Returns the local socket address.
     * @return The local socket address
     * @throws std::bad_alloc  Necessary memory can't be allocated
     * @throws SystemError     Can't get local socket address
     * @threadsafety     Safe
     */
    const InetSockAddr getLocalAddr() const
    {
        struct sockaddr sockAddr;
        socklen_t       len = sizeof(sockAddr);
        if (::getsockname(sd, &sockAddr, &len))
            throw SYSTEM_ERROR("Couldn't get local socket address");
        return InetSockAddr(sockAddr);
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
        remoteSockAddr.setHopLimit(sd, limit);
    }

    /**
     * Scatter-sends a message.
     * @param[in] iovec     Vector comprising message to send
     * @param[in] iovcnt    Number of elements in `iovec`
     * @throws std::system_error  I/O error writing to socket
     */
    void send(
            const struct iovec* iovec,
            const int           iovcnt)
    {
        struct msghdr msghdr = {};
        msghdr.msg_name = &sockAddrStorage;
        msghdr.msg_namelen = sizeof(sockAddrStorage);
        msghdr.msg_iov = const_cast<struct iovec*>(iovec);
        msghdr.msg_iovlen = iovcnt;
#if 0
        ::printf("UdpSock::send(): iovcnt=%d, iovec[0].iov_len=%zu\n",
                iovcnt, iovec[0].iov_len);
#endif
        if (::sendmsg(sd, &msghdr, 0) == -1)
            throw std::system_error(errno, std::system_category(),
                    std::string{"Couldn't send on UDP socket: sd="} +
                    std::to_string(sd));
    }

    /**
     * Sends a message.
     * @param[in] buf       Buffer to send
     * @param[in] len       Size of buffer in bytes
     * @throws std::system_error  I/O error writing to socket
     */
    void send(
            const void* const buf,
            const size_t      len)
    {
        struct iovec iov;
        iov.iov_base = const_cast<void*>(buf);
        iov.iov_len = len;
        send(&iov, 1);
    }
};

/**
 * Multicast UDP socket. The local and remote endpoints of such a socket have
 * the same address, which is a multicast group address. The local endpoint
 * could use the wildcard IP address instead (e.g., htonl(INADDR_ANY), but then
 * the UDP layer would pass to the socket every packet whose destination port
 * number was that of the multicast group, regardless of destination IP address.
 */
class McastUdpSock::Impl final : public InUdpSock::Impl
{
    InetAddr sourceAddr;
    bool     isSourceSpecific;

public:
    /**
     * Constructs a source-independent instance: one that will accept any packet
     * sent to the multicast group from any source.
     * @param[in] mcastAddr  Address of multicast group
     * @param[in] sharePort  Whether or not the local port number should be
     *                       shared amongst multiple sockets
     */
    explicit Impl(
            const InetSockAddr& mcastAddr,
            const bool          sharePort)
        : InUdpSock::Impl(mcastAddr, sharePort) // Calls `::bind()`
        , sourceAddr{}
        , isSourceSpecific{false}
    {
        mcastAddr.joinMcastGroup(sd);
    }

    /**
     * Constructs a source-specific instance: one that will only accept packets
     * from the given source address. The network can establish the routing for
     * such a socket much, much easier than for a source-independent socket.
     * @param[in] mcastAddr   Address of multicast group
     * @param[in] sourceAddr  Address of source
     * @param[in] sharePort   Whether or not the local port number should be
     *                        shared amongst multiple sockets
     */
    Impl(   const InetSockAddr& mcastAddr,
            const InetAddr&     sourceAddr,
            const bool          sharePort)
        : InUdpSock::Impl(mcastAddr, sharePort) // Calls `bind()`
        , sourceAddr{sourceAddr}
        , isSourceSpecific{true}
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
        auto str = std::string("McastUdpSock(addr=") + localSockAddr.to_string()
                + ", sock=" + std::to_string(sd);
        if (isSourceSpecific)
            str = str + ", source=" + sourceAddr.to_string();
        return str + ")";
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
        localSockAddr.setMcastLoop(sd, enable);
    }
};

UdpSock::UdpSock(Impl* const pImpl)
    : pImpl{pImpl}
{}

UdpSock::~UdpSock()
{}

InUdpSock::InUdpSock(Impl* const pImpl)
    : UdpSock{pImpl}
{}

InUdpSock::InUdpSock(
        const InetSockAddr& localSockAddr,
        const bool          sharePort)
    : UdpSock(new Impl(localSockAddr, sharePort))
{}

InUdpSock::~InUdpSock()
{
    /*
     * Non-trivial because `pImpl` stores a pointer-to-void which means that
     * the destructor of the pointed-to UDP socket won't be automatically
     * called.
     */
    if (pImpl && pImpl.unique()) {
        reinterpret_cast<Impl*>(pImpl.get())->~Impl();
        pImpl.reset();
    }
}

InUdpSock::Impl* InUdpSock::getPimpl() const noexcept
{
    return static_cast<Impl*>(pImpl.get());
}

std::string InUdpSock::to_string() const
{
    return getPimpl()->to_string();
}

size_t InUdpSock::recv(
        const struct iovec* iovec,
        const int           iovcnt,
        const bool          peek)
{
    return getPimpl()->recv(iovec, iovcnt, peek);
}

size_t InUdpSock::recv(
        void* const  buf,
        const size_t len,
        const bool   peek)
{
    return getPimpl()->recv(buf, len, peek);
}

void InUdpSock::discard()
{
    return getPimpl()->discard();
}

const InetSockAddr InUdpSock::getLocalAddr() const noexcept
{
    return getPimpl()->getLocalAddr();
}

bool InUdpSock::hasRecord()
{
    return getPimpl()->hasRecord();
}

UdpSock::UdpPayloadSize InUdpSock::getSize() const noexcept
{
    return getPimpl()->getSize();
}

OutUdpSock::Impl* OutUdpSock::getPimpl() const noexcept
{
    return static_cast<Impl*>(pImpl.get());
}

OutUdpSock::OutUdpSock(const InetSockAddr& remoteAddr)
    : UdpSock{new Impl(remoteAddr)}
{}

OutUdpSock::~OutUdpSock()
{
    /*
     * Non-trivial because `pImpl` stores a pointer-to-void which means that
     * the destructor of the pointed-to UDP socket won't be automatically
     * called.
     */
    if (pImpl && pImpl.unique()) {
        reinterpret_cast<Impl*>(pImpl.get())->~Impl();
        pImpl.reset();
    }
}

std::string OutUdpSock::to_string() const
{
    return getPimpl()->to_string();
}

const OutUdpSock& OutUdpSock::setHopLimit(
        const unsigned limit) const
{
    getPimpl()->setHopLimit(limit);
    return *this;
}

void OutUdpSock::send(
        const struct iovec* const iovec,
        const int                 iovcnt)
{
    getPimpl()->send(iovec, iovcnt);
}

const InetSockAddr OutUdpSock::getLocalAddr() const
{
    return getPimpl()->getLocalAddr();
}

void OutUdpSock::send(
        const void* const buf,
        const size_t      len)
{
    getPimpl()->send(buf, len);
}

McastUdpSock::Impl* McastUdpSock::getPimpl() const noexcept
{
    return static_cast<Impl*>(pImpl.get());
}

McastUdpSock::McastUdpSock(
        const InetSockAddr& mcastAddr,
        const bool          sharePort)
    : InUdpSock(new Impl(mcastAddr, sharePort))
{}

McastUdpSock::McastUdpSock(
        const InetSockAddr& mcastAddr,
        const InetAddr&     sourceAddr,
        const bool          sharePort)
    : InUdpSock(new Impl(mcastAddr, sourceAddr, sharePort))
{}

McastUdpSock::~McastUdpSock()
{
    /*
     * Non-trivial because `pImpl` stores a pointer-to-void which means that
     * the destructor of the pointed-to UDP socket won't be automatically
     * called.
     */
    if (pImpl && pImpl.unique()) {
        reinterpret_cast<Impl*>(pImpl.get())->~Impl();
        pImpl.reset();
    }
}

std::string McastUdpSock::to_string() const
{
    return getPimpl()->to_string();
}

} // namespace
