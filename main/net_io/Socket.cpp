/**
 * BSD Sockets.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Socket.cpp
 *  Created on: May 9, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "Socket.h"

#include <atomic>
#include <cstdio>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace hycast {

class Socket::Impl
{
protected:
    int              sd;         ///< Socket descriptor
    std::atomic_bool isShutdown; ///< `shutdown()` has been called?

    explicit Impl(const int sd) noexcept
        : sd{sd}
        , isShutdown{false}
    {}

public:
    virtual ~Impl() noexcept
    {
        ::shutdown(sd, SHUT_RDWR);
        ::close(sd);
    }

    SockAddr getLclAddr() const
    {
        struct sockaddr sockaddr = {};
        socklen_t       socklen = sizeof(sockaddr);

        if (::getsockname(sd, &sockaddr, &socklen))
            throw SYSTEM_ERROR("getsockname() failure on socket " +
                    std::to_string(sd));

        SockAddr sockAddr(sockaddr);
        LOG_DEBUG("%s", sockAddr.to_string().c_str());
        return sockAddr;
    }

    SockAddr getRmtAddr() const
    {
        struct sockaddr sockaddr = {};
        socklen_t       socklen = sizeof(sockaddr);

        if (::getpeername(sd, &sockaddr, &socklen))
            throw SYSTEM_ERROR("getpeername() failure on socket " +
                    std::to_string(sd));

        return SockAddr(sockaddr);
    }

    in_port_t getLclPort() const
    {
        struct sockaddr sockaddr = {};
        socklen_t       socklen = sizeof(sockaddr);
        in_port_t       port;

        if (::getsockname(sd, &sockaddr, &socklen))
            throw SYSTEM_ERROR("getsockname() failure on socket " +
                    std::to_string(sd));

        if (sockaddr.sa_family == AF_INET) {
            struct sockaddr_in* addr =
                    reinterpret_cast<struct sockaddr_in*>(&sockaddr);

            port = ntohs(addr->sin_port);
        }
        else if (sockaddr.sa_family == AF_INET6) {
            struct sockaddr_in6* addr =
                    reinterpret_cast<struct sockaddr_in6*>(&sockaddr);

            port = ntohs(addr->sin6_port);

        }
        else {
            throw RUNTIME_ERROR("Unsupported address family: " +
                    std::to_string(sockaddr.sa_family));
        }

        return port;
    }

    in_port_t getRmtPort() const
    {
        return getRmtAddr().getPort();
    }

    /**
     * Shuts down the socket for both reading and writing. Causes `accept()` to
     * throw an exception. Idempotent.
     */
    void shutdown()
    {
        isShutdown = true;
        ::shutdown(sd, SHUT_RDWR);
    }
};

Socket::Socket(Impl* impl)
    : pImpl{impl}
{}

Socket::~Socket()
{}

SockAddr Socket::getLclAddr() const
{
    SockAddr sockAddr(pImpl->getLclAddr());
    LOG_DEBUG("%s", sockAddr.to_string().c_str());
    return sockAddr;
}

in_port_t Socket::getLclPort() const
{
    return pImpl->getLclPort();
}

SockAddr Socket::getRmtAddr() const
{
    return pImpl->getRmtAddr();
}

in_port_t Socket::getRmtPort() const
{
    return pImpl->getRmtPort();
}

/******************************************************************************/

class InetSock::Impl : public Socket::Impl
{
protected:
    /**
     * Constructs.
     *
     * @param[in] sd       Socket descriptor
     * @exceptionsafety    Strong guarantee
     * @cancellationpoint
     */
    Impl(const int sd)
        : Socket::Impl(sd)
    {}

public:
    virtual ~Impl() noexcept
    {}
};

InetSock::InetSock(Impl* impl)
    : Socket{impl}
{}

InetSock::~InetSock()
{}

/******************************************************************************/

class TcpSock::Impl : public InetSock::Impl
{
protected:
    friend class TcpSrvrSock;

    Impl();

    /**
     * Constructs.
     *
     * @param[in] sd       Socket descriptor
     * @exceptionsafety    Strong guarantee
     * @cancellationpoint
     */
    Impl(const int sd)
        : InetSock::Impl{sd}
    {}

public:
    std::string to_string() const
    {
        return "{rmtAddr: " + getRmtAddr().to_string() + ", lclAddr: " +
                getLclAddr().to_string() + "}";
    }

    /**
     * Sets the Nagle algorithm.
     *
     * @param[in] enable             Whether or not to enable the Nagle
     *                               algorithm
     * @throws    std::system_error  `setsockopt()` failure
     */
    void setDelay(int enable)
    {
        if (::setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, &enable,
                sizeof(enable))) {
            throw SYSTEM_ERROR("Couldn't set TCP_NODELAY to %d on socket " +
                    std::to_string(enable) + std::to_string(sd));
        }
    }

    void write(
            const void* bytes,
            size_t      nbytes) const
    {
        LOG_DEBUG("Writing %zu bytes", nbytes);
        if (::write(sd, bytes, nbytes) != nbytes)
            throw SYSTEM_ERROR("Couldn't write " + std::to_string(nbytes) +
                    " bytes to host " + getRmtAddr().to_string());
    }

    void write(uint16_t value)
    {
        value = hton(value);
        write(&value, sizeof(value));
    }

    void write(uint32_t value)
    {
        value = hton(value);
        write(&value, sizeof(value));
    }

    void write(uint64_t value)
    {
        value = hton(value);
        write(&value, sizeof(value));
    }

    /**
     * Reads from the socket. No network-to-host translation is performed.
     *
     * @param[in] nbytes       Maximum amount of data to read in bytes
     * @return                 Number of bytes actually read. 0 => EOF.
     * @throw     SystemError  I/O failure
     */
    size_t read(
            void* const  data,
            const size_t nbytes) const
    {
        /*
         * Sending process closes connection => FIN sent => EOF
         * Sending process crashes => FIN sent => EOF
         * Sending host crashes => read() won't return
         * Sending host becomes unreachable => read() won't return
         */
        LOG_DEBUG("Reading %zu bytes", nbytes);

        size_t nleft = nbytes;

        while (nleft) {
            ssize_t nread = ::read(sd, data, nbytes);

            if (nread == -1)
                throw SYSTEM_ERROR("read() failure on host " +
                        getRmtAddr().to_string());

            if (nread == 0)
                break; // EOF

            nleft -= nread;
        }

        return nbytes - nleft;
    }

    /**
     * @param[out] value
     * @retval     `true`   Success
     * @retval     `false`  EOF
     */
    bool read(uint16_t& value)
    {
        if (read(&value, sizeof(value)) != sizeof(value))
            return false;
        value = ntoh(value);
        return true;
    }

    /**
     * @param[out] value
     * @retval     `true`   Success
     * @retval     `false`  EOF
     */
    bool read(uint32_t& value)
    {
        if (read(&value, sizeof(value)) != sizeof(value))
            return false;
        value = ntoh(value);
        return true;
    }

    /**
     * @param[out] value
     * @retval     `true`   Success
     * @retval     `false`  EOF
     */
    bool read(uint64_t& value)
    {
        if (read(&value, sizeof(value)) != sizeof(value))
            return false;
        value = ntoh(value);
        return true;
    }
};

TcpSock::TcpSock(Impl* impl)
    : InetSock{impl}
{}

TcpSock::~TcpSock()
{}

std::string TcpSock::to_string() const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->to_string();
}

TcpSock& TcpSock::setDelay(bool enable)
{
    static_cast<TcpSock::Impl*>(pImpl.get())->setDelay(enable);
    return *this;
}

void TcpSock::write(
        const void* bytes,
        size_t      nbytes) const
{
    static_cast<TcpSock::Impl*>(pImpl.get())->write(bytes, nbytes);
}

void TcpSock::write(uint16_t value) const
{
    static_cast<TcpSock::Impl*>(pImpl.get())->write(value);
}

void TcpSock::write(uint32_t value) const
{
    static_cast<TcpSock::Impl*>(pImpl.get())->write(value);
}

void TcpSock::write(uint64_t value) const
{
    static_cast<TcpSock::Impl*>(pImpl.get())->write(value);
}

size_t TcpSock::read(
        void* const  bytes,
        const size_t nbytes) const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->read(bytes, nbytes);
}

bool TcpSock::read(uint16_t& value) const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->read(value);
}

bool TcpSock::read(uint32_t& value) const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->read(value);
}

bool TcpSock::read(uint64_t& value) const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->read(value);
}

void TcpSock::shutdown() const
{
    return static_cast<TcpSock::Impl*>(pImpl.get())->shutdown();
}

/******************************************************************************/

class TcpSrvrSock::Impl final : public TcpSock::Impl
{
public:
    /**
     * Constructs. Calls `::listen()`.
     *
     * @param[in] sockAddr           Server's local socket address
     * @param[in] queueSize          Size of listening queue
     * @throws    std::system_error  Couldn't set SO_REUSEADDR on socket
     * @throws    std::system_error  Couldn't bind socket to `sockAddr`
     * @throws    std::system_error  Couldn't set SO_KEEPALIVE on socket
     * @throws    std::system_error  `::listen()` failure
     */
    Impl(   const SockAddr& sockAddr,
            const int       queueSize)
        : TcpSock::Impl{sockAddr.socket(SOCK_STREAM)}
    {
        const int enable = 1;

        LOG_DEBUG("Setting SO_REUSEADDR");
        if (::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)))
            throw SYSTEM_ERROR("Couldn't set SO_REUSEADDR on socket " +
                    std::to_string(sd) + ", address " + sockAddr.to_string());

        LOG_DEBUG("Binding socket");
        sockAddr.bind(sd);

        LOG_DEBUG("Setting SO_KEEPALIVE");
        if (::setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)))
            throw SYSTEM_ERROR("Couldn't set SO_KEEPALIVE on socket " +
                    std::to_string(sd) + ", address " + sockAddr.to_string());

        if (::listen(sd, queueSize))
            throw SYSTEM_ERROR("listen() failure: {sock: " + std::to_string(sd)
                    + ", queueSize: " + std::to_string(queueSize) + "}");
    }

    std::string to_string() const
    {
        return getLclAddr().to_string();
    }

    /**
     * Accepts an incoming connection. Calls `::accept()`.
     *
     * @retval  `nullptr`          `shutdown()` was called
     * @return                     The accepted socket
     * @throws  std::system_error  `::accept()` failure
     * @cancellationpoint          Yes
     */
    TcpSock::Impl* accept()
    {
        const int fd = ::accept(sd, nullptr, nullptr);

        if (fd == -1) {
            if (isShutdown)
                return nullptr;

            throw SYSTEM_ERROR("accept() failure on socket " +
                        std::to_string(sd));
        }

        return new TcpSock::Impl(fd);
    }
};

TcpSrvrSock::TcpSrvrSock(
        const SockAddr& sockAddr,
        const int       queueSize)
    : TcpSock{new Impl(sockAddr, queueSize)}
{}

std::string TcpSrvrSock::to_string() const
{
    return static_cast<Impl*>(pImpl.get())->to_string();
}

TcpSock TcpSrvrSock::accept() const
{
    return TcpSock{static_cast<TcpSrvrSock::Impl*>(pImpl.get())->accept()};
}

/******************************************************************************/

class TcpClntSock::Impl final : public TcpSock::Impl
{
public:
    /**
     * Constructs.
     *
     * @param[in] sockAddr           Address of remote endpoint
     * @throw     std::system_error  Couldn't connect to `sockAddr`
     * @exceptionsafety              Strong guarantee
     * @cancellationpoint
     */
    Impl(const SockAddr& sockAddr)
        : TcpSock::Impl(sockAddr.socket(SOCK_STREAM, IPPROTO_TCP))
    {
        sockAddr.connect(sd);
    }
};

TcpClntSock::TcpClntSock(const SockAddr& sockAddr)
    : TcpSock(new Impl(sockAddr))
{}

/******************************************************************************/

class UdpSock::Impl final : public InetSock::Impl
{
    size_t   numPeeked; ///< Number of bytes peeked

public:
    /**
     * @cancellationpoint
     */
    Impl(const SockAddr& grpAddr)
        : InetSock::Impl(grpAddr.socket(SOCK_DGRAM, IPPROTO_UDP))
        , numPeeked{0}
    {
        unsigned char  ttl = 250; // Should be large enough

        if (::setsockopt(sd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)))
            throw SYSTEM_ERROR(
                    "Couldn't set time-to-live for multicast packets");

        // Enable loopback of multicast datagrams
        {
            unsigned char enable = 1;
            if (::setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, &enable,
                    sizeof(enable)))
                throw SYSTEM_ERROR(
                        "Couldn't enable loopback of multicast datagrams");
        }

        grpAddr.connect(sd);
    }

    /**
     * @cancellationpoint
     */
    Impl(   const SockAddr& grpAddr,
            const InetAddr& rmtAddr)
        : InetSock::Impl(grpAddr.socket(SOCK_DGRAM, IPPROTO_UDP))
        , numPeeked{0}
    {
        grpAddr.bind(sd);
        grpAddr.getInetAddr().join(sd, rmtAddr);
    }

    /**
     * Sets the interface to be used by the UDP socket for multicasting. The
     * default is system dependent.
     */
    void setMcastIface(const InetAddr& iface) const
    {
        iface.setMcastIface(sd);
    }

    std::string to_string() const
    {
        return getLclAddr().to_string();
    }

    void write(
            const struct iovec* iov,
            const int           iovCnt)
    {
        if (::writev(sd, iov, iovCnt) == -1)
            throw SYSTEM_ERROR("Couldn't write " + std::to_string(iovCnt) +
                    "-element vector to host " + getRmtAddr().to_string());
    }

    void peek(
            void*        bytes,
            const size_t nbytes)
    {
        ssize_t nread = ::recv(sd, bytes, nbytes, MSG_PEEK);

        if (nread < 0)
            throw SYSTEM_ERROR("::recv() failure");

        if (nread != nbytes)
            throw EOF_ERROR("Read " +
                    std::to_string(nread) + " bytes from host " +
                    getRmtAddr().to_string() + " but expected " +
                    std::to_string(nbytes));
    }

    /**
     * Reads a UDP record sequentially (i.e., previously read bytes are
     * skipped). No network-to-host translation is performed.
     *
     * @param[in] iov           I/O vector
     * @return                  Number of new bytes read. 0 => EOF.
     * @throws    SystemError   I/O error
     * @throws    RuntimeError  Packet is too small
     * @cancellationpoint       Yes
     */
    size_t read(
            const struct iovec* iov,
            const int           iovCnt)
    {
        // Construct new read vector that skips previously peeked bytes
        struct iovec  myIov[iovCnt+1];
        size_t        reqNumPeek = 0; // Requested number of bytes to read
        char          skipBuf[numPeeked];
        myIov[0].iov_base = skipBuf;
        myIov[0].iov_len = numPeeked;
        for (int i = 0; i < iovCnt; ++i) {
            myIov[i+1] = iov[i];
            reqNumPeek += iov[i].iov_len;
        }

        // Construct recvmsg() control structure
        struct msghdr msghdr = {};
        msghdr.msg_name = msghdr.msg_control = nullptr;
        msghdr.msg_iov = myIov;
        msghdr.msg_iovlen = iovCnt+1;

        // Peek packet
        ssize_t nread = ::recvmsg(sd, &msghdr, MSG_PEEK);
        if (nread < 0)
            throw SYSTEM_ERROR("recvmsg() failure");

        nread -= numPeeked;
        numPeeked += nread;

        return nread;
    }

    void discard()
    {
        char byte;
        ::read(sd, &byte, sizeof(byte)); // Discards packet
        numPeeked = 0; // For next time
    }

    void shutdown(const int how)
    {
        const int status = ::shutdown(sd, how);
        if (status == EINVAL)
            throw INVALID_ARGUMENT("how: " + std::to_string(how));
        if (status && errno != ENOTCONN)
            throw SYSTEM_ERROR("shutdown() failure");
    }
};

UdpSock::UdpSock(const SockAddr& grpAddr)
    : InetSock{new Impl(grpAddr)}
{}

UdpSock::UdpSock(
        const SockAddr& grpAddr,
        const InetAddr& rmtAddr)
    : InetSock{new Impl(grpAddr, rmtAddr)}
{}

const UdpSock& UdpSock::setMcastIface(const InetAddr& iface) const
{
    static_cast<UdpSock::Impl*>(pImpl.get())->setMcastIface(iface);
    return *this;
}

std::string UdpSock::to_string() const
{
    return static_cast<UdpSock::Impl*>(pImpl.get())->to_string();
}

void UdpSock::write(
        const struct iovec* iov,
        const int           iovCnt)
{
    static_cast<UdpSock::Impl*>(pImpl.get())->write(iov, iovCnt);
}

size_t UdpSock::read(
        void*        bytes,
        const size_t nbytes)
{
    struct iovec iov;
    iov.iov_base = bytes;
    iov.iov_len = nbytes;
    return static_cast<UdpSock::Impl*>(pImpl.get())->read(&iov, 1);
}

size_t UdpSock::read(
        const struct iovec* iov,
        const int           iovCnt)
{
    return static_cast<UdpSock::Impl*>(pImpl.get())->read(iov, iovCnt);
}

void UdpSock::discard()
{
    static_cast<UdpSock::Impl*>(pImpl.get())->discard();
}

void UdpSock::shutdown(const int how)
{
    static_cast<UdpSock::Impl*>(pImpl.get())->shutdown(how);
}

} // namespace
