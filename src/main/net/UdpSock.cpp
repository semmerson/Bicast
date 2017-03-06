/**
 * This file implements handle classes for UDP sockets and UDP sockets.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: UdpSock.cpp
 * @author: Steven R. Emmerson
 */

#include "error.h"
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
    bool         haveCurrRec; /// Current record exists?
    struct iovec sizeField;   /// Scatter-read element for size field
    uint16_t     size;        /// Size of payload in bytes (excludes size field)

    void init()
    {
        haveCurrRec = false;
        size = 0;
        sizeField.iov_base = &size;
        sizeField.iov_len = sizeof(size);
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
            throw SystemError(__FILE__, __LINE__,
                    "Couldn't share local port number for incoming packets: "
                    "sock=" + std::to_string(sd));
    }

    /**
     * @pre `size` is set in network byte order
     */
    void checkReadStatus(const ssize_t nbytes)
    {
        if (nbytes < 0) {
            if (errno == ECONNRESET || errno == ENOTCONN) {
                size = 0; // EOF
            }
            else {
                throw SystemError(__FILE__, __LINE__,
                        std::string{"recv() failure: sock="} +
                        std::to_string(sd));
            }
        }
        else {
            size = ntohs(size);
            if (static_cast<size_t>(nbytes) != sizeField.iov_len + size)
                throw std::runtime_error(
                        std::string{"Invalid UDP read: nbytes="} +
                        std::to_string(nbytes) + ", size=" +
                        std::to_string(size) + ", sock=" +
                        std::to_string(sd));
        }
    }

    void ensureRec()
    {
        if (!haveCurrRec) {
            try {
                ssize_t nbytes = ::recv(sd, sizeField.iov_base, sizeField.iov_len,
                        MSG_PEEK);
                checkReadStatus(nbytes);
                haveCurrRec = true;
            }
            catch (const std::exception& e) {
                std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                        "Couldn't receive next UDP packet"));
            }
        }
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
        init();
        if (sharePort)
            shareLocalPort(); // Must be before `bind()`
        bind(); // Set address of local endpoint
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
     * Returns the size, in bytes, of the current record. Waits for the
     * record if necessary. The record is left in the socket's input buffer.
     * @retval 0  Socket is closed
     * @return Size of record in bytes
     * @throws std::system_error I/O error occurred
     * @exceptionsafety Basic
     */
    size_t getSize()
    {
        ensureRec();
        return size;
    }

    /**
     * Scatter-receives a record. Waits for the record if necessary. If the
     * requested number of bytes to be read is less than the record size, then
     * the excess bytes are discarded.
     * @param[in] iovec   Scatter-read vector
     * @param[in] iovcnt  Number of elements in scatter-read vector
     * @param[in] peek    Whether or not to peek at the record. The data is
     *                    treated as unread and the next recv() or similar
     *                    function shall still return this data.
     * @retval    0       Socket is closed
     * @return            Actual number of bytes read into the buffers.
     * @throws std::system_error  I/O error reading from socket */
    size_t recv(
           struct iovec* iovec,
           const int     iovcnt,
           const bool    peek = false)
    {
        struct iovec iov[1+iovcnt];
        iov[0] = sizeField;
        for (int i = 0; i < iovcnt; ++i)
            iov[1+i] = iovec[i];
        struct msghdr msghdr = {};
        msghdr.msg_iov = const_cast<struct iovec*>(iov);
        msghdr.msg_iovlen = 1 + iovcnt;
        ssize_t nbytes = ::recvmsg(sd, &msghdr, peek ? MSG_PEEK : 0);
        checkReadStatus(nbytes);
        haveCurrRec = peek;
        return size;
    }

    /**
     * Receives a record. Waits for the record if necessary. If the requested
     * number of bytes to be read is less than the record size, then the excess
     * bytes are discarded.
     * @param[in] buf     Receive buffer
     * @param[in] len     Size of receive buffer in bytes
     * @param[in] peek    Whether or not to peek at the record. The data is
     *                    treated as unread and the next recv() or similar
     *                    function shall still return this data.
     * @retval    0       Socket is closed
     * @return            Actual number of bytes read into the buffer.
     * @throws std::system_error  I/O error reading from socket */
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
     * Discards the current record. Does nothing if there is no current
     * record. Idempotent.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Thread-compatible but not thread-safe
     */
    void discard()
    {
        if (haveCurrRec) {
            char         buf;
            recv(&buf, sizeof(buf));
        }
    }

    /**
     * Indicates if there's a current record.
     */
    bool hasRecord()
    {
        return haveCurrRec;
    }
};

/**
 * Output UDP socket.
 */
class OutUdpSock::Impl final : public UdpSock::Impl
{
    struct iovec            sizeField;
    uint16_t                size;
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
        sizeField.iov_base = &size;
        sizeField.iov_len = sizeof(size);
        remoteSockAddr.setSockAddrStorage(sd, sockAddrStorage);
        /*
         * Sets address of remote endpoint for function `::send()` (but not
         * `::sendmsg()`)
         */
        remoteSockAddr.connect(sd);
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
            throw SystemError(__FILE__, __LINE__,
                    "Couldn't get local socket address");
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
     * Scatter-sends a message. The size of the payload in bytes is encoded in
     * the first two bytes of the packet.
     * @param[in] iovec     Vector comprising message to send
     * @param[in] iovcnt    Number of elements in `iovec`
     * @throws std::system_error  I/O error writing to socket
     */
    void send(
            const struct iovec* iovec,
            const int           iovcnt)
    {
        size = 0;
        for (int i = 0; i < iovcnt; ++i)
            size += iovec[i].iov_len;
        struct iovec iov[1+iovcnt];
        size = htons(size);
        iov[0] = sizeField;
        for (int i = 0; i < iovcnt; ++i)
            iov[1+i] = iovec[i];
        struct msghdr msghdr = {};
        msghdr.msg_name = &sockAddrStorage;
        msghdr.msg_namelen = sizeof(sockAddrStorage);
        msghdr.msg_iov = const_cast<struct iovec*>(iov);
        msghdr.msg_iovlen = 1+iovcnt;
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
        struct iovec* iovec,
        const int     iovcnt,
        const bool    peek)
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

size_t InUdpSock::getSize()
{
    return getPimpl()->getSize();
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
