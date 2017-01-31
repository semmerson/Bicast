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
#include <sys/uio.h>
#include <system_error>
#include <unistd.h>

namespace hycast {

/**
 * Base UDP socket implementation.
 */
class UdpSock::Impl
{
protected:
    InetSockAddr inetSockAddr; /// Internet socket address of endpoint
    int          sd;           /// Socket descriptor

public:
    /**
     * Default constructs.
     */
    Impl() // Deliberately doesn't initialize the socket descriptor
    {}

    /**
     * Constructs from the Internet socket address of an endpoint.
     * @param[in] localAddr   Internet socket address of an endpoint
     * @throws std::system_error  Socket couldn't be created
     */
    explicit Impl(const InetSockAddr& inetSockAddr)
        : inetSockAddr{inetSockAddr}
        , sd{inetSockAddr.getSocket(SOCK_DGRAM)}
    {}

    /**
     * Destroys. Closes the underlying socket.
     */
    ~Impl() noexcept
    {
        ::close(sd);
    }
};

/**
 * Input UDP socket implementation.
 */
class InUdpSock::Impl : virtual public UdpSock::Impl, public InRecStream
{
    bool         haveCurrRec; /// Current record exists?
    uint16_t     size;        /// Size of payload in bytes

    void checkReadStatus(const ssize_t nbytes)
    {
        if (nbytes == 0) {
            size = 0; // EOF
        }
        else if (nbytes == -1) {
            if (errno == ECONNRESET || errno == ENOTCONN) {
                size = 0; // EOF
            }
            else {
                throw std::system_error(errno, std::system_category(),
                        "recv() failure: sock=" + std::to_string(sd));
            }
        }
        else if (nbytes != sizeof(size)) {
            throw std::system_error(errno, std::system_category(),
                    "recv() read too few bytes: nbytes=" +
                    std::to_string(nbytes) + ", sock=" + std::to_string(sd));
        }
        else {
            size = ntohs(size);
        }
    }

    void ensureRec()
    {
        if (!haveCurrRec) {
            ssize_t nbytes = ::recv(sd, &size, sizeof(size), MSG_PEEK);
            checkReadStatus(nbytes);
            haveCurrRec = true;
        }
    }

protected:
    void bind()
    {
        inetSockAddr.bind(sd);
    }

public:
    using InRecStream::recv;

    /**
     * Default constructs.
     */
    Impl()
        : UdpSock::Impl()
        , haveCurrRec{false}
        , size{0}
    {}

    /**
     * Constructs from the Internet socket address of the local endpoint.
     * @param[in] localAddr  Internet socket address of local endpoint
     */
    Impl(const InetSockAddr& localAddr)
        : UdpSock::Impl(localAddr)
        , haveCurrRec{false}
        , size{0}
    {
        bind(); // Set address of local endpoint
    }

    std::string to_string() const
    {
        return std::string("InUdpSock(localAddr=") + inetSockAddr.to_string() +
                ", sock=" + std::to_string(sd) + ")";
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
            throw std::system_error(errno, std::system_category(),
                   "setsockopt() failure: couldn't share port-number");
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
     * Receives a record. Waits for the record if necessary. If the requested
     * number of bytes to be read is less than the record size, then the excess
     * bytes are discarded.
     * @param[in] iovec   Scatter-read vector
     * @param[in] iovcnt  Number of elements in scatter-read vector
     * @param[in] peek    Whether or not to peek at the record. The data is
     *                    treated as unread and the next recv() or similar
     *                    function shall still return this data.
     * @retval    0       Socket is closed
     * @return            Actual number of bytes read into the buffers.
     */
    size_t recv(
           const struct iovec* iovec,
           const int           iovcnt,
           const bool          peek)
    {
        struct iovec iov[1+iovcnt];
        iov[0].iov_base = &size;
        iov[0].iov_len = sizeof(size);
        for (int i = 0; i < iovcnt; ++i)
            iov[1+i] = iovec[i];
        struct msghdr msghdr = {};
        msghdr.msg_iov = iov;
        msghdr.msg_iovlen = 1+iovcnt;
        ssize_t nbytes = ::recvmsg(sd, &msghdr, peek ? MSG_PEEK : 0);
        checkReadStatus(nbytes);
        haveCurrRec = peek;
        return nbytes;
    }

    /**
     * Discards the current record. Does nothing if there is no current
     * record. Idempotent.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Thread-compatible but not thread-safe
     */
    void discard()
    {
        char buf;
        InRecStream::recv(&buf, sizeof(buf), false);
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
class OutUdpSock::Impl : virtual public UdpSock::Impl, public OutRecStream
{
protected:
    void connect()
    {
        inetSockAddr.connect(sd);
    }

    /**
     * Default constructs.
     */
    Impl()
    {}

public:
    using OutRecStream::send;

    /**
     * Constructs from Internet address of remote endpoint.
     * @param[in] remoteAddr   Remote endpoint address
     */
    Impl(   const InetSockAddr& remoteAddr)
        : UdpSock::Impl{remoteAddr}
    {
        connect(); // Set remote endpoint for outgoing packets
    }

    std::string to_string() const
    {
        return std::string("OutUdpSock(remoteAddr=") + inetSockAddr.to_string() +
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
        inetSockAddr.setHopLimit(sd, limit);
    }

    void send(
            const struct iovec* iovec,
            const int           iovcnt)
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
class McastUdpSock::Impl : public InUdpSock::Impl, public OutUdpSock::Impl
{
    InetAddr sourceAddr;
    bool     isSourceSpecific;

public:
    /**
     * Constructs a source-independent instance: one that will accept any packet
     * sent to the multicast group from any source.
     * @param[in] mcastAddr   Address of multicast group
     */
    Impl(const InetSockAddr& mcastAddr)
        : UdpSock::Impl{mcastAddr}
        , isSourceSpecific{false}
    {
        bind();
        connect();
        mcastAddr.joinMcastGroup(sd);
    }

    /**
     * Constructs a source-specific instance: one that will only accept packets
     * from the given source address. The routing for such a socket is much
     * easier for the network to handle.
     * @param[in] mcastAddr   Address of multicast group
     * @param[in] sourceAddr  Address of source
     */
    Impl(   const InetSockAddr& mcastAddr,
            const InetAddr&     sourceAddr)
        : UdpSock::Impl{mcastAddr}
        , sourceAddr{sourceAddr}
        , isSourceSpecific{true}
    {
        bind();
        connect();
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
        return std::string("McastUdpSock(mcastAddr=") + inetSockAddr.to_string() +
                ", sock=" + std::to_string(sd) + ", source=" +
                sourceAddr.to_string() + ")";
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
        inetSockAddr.setMcastLoop(sd, enable);
    }
};

UdpSock::UdpSock(Impl* const pImpl)
    : pImpl{pImpl}
{}

InUdpSock::InUdpSock(const InetSockAddr& localAddr)
    : UdpSock(new InUdpSock::Impl(localAddr))
{}

void InUdpSock::shareLocalPort() const
{
    getPimpl()->shareLocalPort();
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

size_t InUdpSock::getSize()
{
    return getPimpl()->getSize();
}

void InUdpSock::discard()
{
    return getPimpl()->discard();
}

bool InUdpSock::hasRecord()
{
    return getPimpl()->hasRecord();
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

OutUdpSock::OutUdpSock(const InetSockAddr& remoteAddr)
    : UdpSock(new OutUdpSock::Impl(remoteAddr))
{}

McastUdpSock::McastUdpSock(const InetSockAddr& mcastAddr)
    : UdpSock(new McastUdpSock::Impl(mcastAddr))
{}

McastUdpSock::McastUdpSock(
            const InetSockAddr& mcastAddr,
            const InetAddr&     sourceAddr)
    : UdpSock(new McastUdpSock::Impl(mcastAddr, sourceAddr))
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
