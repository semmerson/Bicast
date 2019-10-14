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

#include <cstdio>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace hycast {

class Socket::Impl
{
protected:
    int sd; ///< Socket descriptor

    explicit Impl(const int sd) noexcept
        : sd{sd}
    {}

public:
    virtual ~Impl() noexcept
    {
        ::shutdown(sd, SHUT_RDWR);
        ::close(sd);
    }

    const SockAddr getLclAddr() const
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

    const SockAddr getRmtAddr() const
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
    void shutdown() const
    {
        ::shutdown(sd, SHUT_RDWR);
    }
};

Socket::Socket(Impl* impl)
    : pImpl{impl}
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
     * Reads from the socket.
     *
     * @param[in] nbytes             Maximum amount of data to read in bytes
     * @return                       Number of bytes actually read. 0 => EOF
     * @throw     std::system_error  I/O failure
     */
    size_t read(
            void* const  data,
            size_t       nbytes) const
    {
        /*
         * Sending process closes connection => FIN sent => EOF
         * Sending process crashes => FIN sent => EOF
         * Sending host crashes => read() won't return
         * Sending host becomes unreachable => read() won't return
         */
        LOG_DEBUG("Reading %zu bytes", nbytes);
        ssize_t nread = ::read(sd, data, nbytes);

        if (nread == -1)
            throw SYSTEM_ERROR("read() failure on host " +
                    getRmtAddr().to_string());

        return nread;
    }

    bool read(uint16_t& value)
    {
        if (read(&value, sizeof(value)) != sizeof(value))
            return false;
        value = ntoh(value);
        return true;
    }

    bool read(uint32_t& value)
    {
        if (read(&value, sizeof(value)) != sizeof(value))
            return false;
        value = ntoh(value);
        return true;
    }

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
     * Constructs.
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

    /**
     * Accepts an incoming connection. Calls `::accept()`.
     *
     * @return                     The accepted socket
     * @throws  std::system_error  `::accept()` failure
     * @cancellationpoint
     */
    TcpSock::Impl* accept()
    {
        const int fd = ::accept(sd, nullptr, nullptr);

        if (fd == -1)
            throw SYSTEM_ERROR("accept() failure on socket " +
                    std::to_string(sd));

        return new TcpSock::Impl(fd);
    }
};

TcpSrvrSock::TcpSrvrSock(
        const SockAddr& sockAddr,
        const int       queueSize)
    : TcpSock{new Impl(sockAddr, queueSize)}
{}

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

class UdpSndrSock::Impl final : public InetSock::Impl
{
public:
    /**
     * @cancellationpoint
     */
    Impl(   const SockAddr& ifAddr,
            const SockAddr& grpAddr)
        : InetSock::Impl(0) // TODO
    {
        // TODO
    }

    void write(
            const struct iovec* iov,
            const int           iovCnt)
    {
#if 0
        if (log_enabled(LOG_LEVEL_DEBUG)) {
            size_t nbytes = 0;
            for (int i = 0; i < iovCnt; ++i)
                nbytes += iov->iov_len;
            LOG_DEBUG("Writing %zu bytes", nbytes);
        }
#endif

        if (::writev(sd, iov, iovCnt) == -1)
            throw SYSTEM_ERROR("Couldn't write " + std::to_string(iovCnt) +
                    "-element vector to host " + getRmtAddr().to_string());
    }

};

UdpSndrSock::UdpSndrSock(
        const SockAddr& ifAddr,
        const SockAddr& grpAddr)
    : InetSock{new Impl(ifAddr, grpAddr)}
{}

void UdpSndrSock::write(
        const struct iovec* iov,
        const int           iovCnt)
{
    static_cast<UdpSndrSock::Impl*>(pImpl.get())->write(iov, iovCnt);
}

/******************************************************************************/

class UdpRcvrSock::Impl final : public InetSock::Impl
{
public:
    /**
     * @cancellationpoint
     */
    Impl(   const SockAddr& ifAddr,
            const SockAddr& grpAddr,
            const SockAddr& srcAddr)
        : InetSock::Impl(0) // TODO
    {}

    size_t peek(
            void*        bytes,
            const size_t nbytes)
    {
        ssize_t nread = ::recv(sd, bytes, nbytes, MSG_PEEK);

        if (nread < 0)
            throw SYSTEM_ERROR("::recv() failure: read " +
                    std::to_string(nread) + " bytes from host " +
                    getRmtAddr().to_string() + " but expected " +
                    std::to_string(nbytes));

        return nread;
    }

    size_t read(
            const struct iovec* iov,
            const int           iovCnt)
    {
        size_t nbytes = 0;
        for (int i = 0; i < iovCnt; ++i)
            nbytes += iov->iov_len;

        LOG_DEBUG("Reading %zu bytes", nbytes);

        auto nread = ::readv(sd, iov, iovCnt);

        if (nread < 0)
            throw SYSTEM_ERROR("::readv() failure: read " +
                    std::to_string(nread) + " bytes from host " +
                    getRmtAddr().to_string() + " but expected " +
                    std::to_string(nbytes));

        return nread;
    }
};

UdpRcvrSock::UdpRcvrSock(
        const SockAddr& ifAddr,
        const SockAddr& grpAddr,
        const SockAddr& srcAddr)
    : InetSock{new Impl(ifAddr, grpAddr, srcAddr)}
{}

size_t UdpRcvrSock::peek(
        void*        bytes,
        const size_t nbytes)
{
    return static_cast<UdpRcvrSock::Impl*>(pImpl.get())->peek(bytes, nbytes);
}

size_t UdpRcvrSock::read(
        const struct iovec* iov,
        const int           iovCnt)
{
    return static_cast<UdpRcvrSock::Impl*>(pImpl.get())->read(iov, iovCnt);
}

} // namespace
