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
    int  sd;         ///< Socket descriptor
    bool byteStream; ///< Implements a byte-stream?

    /**
     * Creates a streaming socket.
     *
     * @param[in] sockAddr           Socket address. Only the address family is
     *                               used.
     * @throws    std::system_error  Couldn't create socket
     */
    explicit Impl(const SockAddr& sockAddr)
        : sd{sockAddr.socket(SOCK_STREAM)}
    {}

public:
    explicit Impl(const int sd) noexcept
        : sd{sd}
    {
        int       type;
        socklen_t typeLen = sizeof(type);
        int       status = ::getsockopt(sd, SOL_SOCKET, SO_TYPE, &type,
                &typeLen);

        if (status)
            throw SYSTEM_ERROR("Couldn't get socket type");

        byteStream = type == SOCK_STREAM;
    }

    virtual ~Impl() noexcept
    {
        ::shutdown(sd, SHUT_RDWR);
        ::close(sd);
    }

    bool isByteStream()
    {
        return byteStream;
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

    /**
     * Returns the setting of the Nagle algorithm.
     *
     * @retval `true`             The Nagle algorithm is enabled
     * @retval `false`            The Nagle algorithm is not enabled
     * @throws std::system_error  `getsockopt()` failure
     */
    bool getDelay()
    {
        int       enabled;
        socklen_t len = sizeof(enabled);

        if (::getsockopt(sd, IPPROTO_TCP, TCP_NODELAY, &enabled, &len)) {
            throw SYSTEM_ERROR("Couldn't get TCP_NODELAY setting on socket " +
                    std::to_string(sd));
        }

        return enabled;
    }

    const SockAddr getAddr() const
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

    const SockAddr getPeerAddr() const
    {
        struct sockaddr sockaddr = {};
        socklen_t       socklen = sizeof(sockaddr);

        if (::getpeername(sd, &sockaddr, &socklen))
            throw SYSTEM_ERROR("getpeername() failure on socket " +
                    std::to_string(sd));

        return SockAddr(sockaddr);
    }

    in_port_t getPort() const
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

    in_port_t getPeerPort() const
    {
        return getPeerAddr().getPort();
    }

    /**
     * Reads from the socket.
     *
     * @param[in] nbytes             Maximum amount of data to read in bytes
     * @retval    0                  EOF
     * @return                       Number of bytes actually read
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
        ssize_t status = ::read(sd, data, nbytes);

        if (status < 0)
            throw SYSTEM_ERROR("read() failure on host " +
                    getPeerAddr().to_string());

        return status;
    }

    void write(
            const void* data,
            size_t      nbytes) const
    {
        LOG_DEBUG("Writing %zu bytes", nbytes);
        if (::write(sd, data, nbytes) != nbytes)
            throw SYSTEM_ERROR("Couldn't write " + std::to_string(nbytes) +
                    " bytes to host " + getPeerAddr().to_string());
    }

    void writev(
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
                    "-element vector to host " + getPeerAddr().to_string());
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

/******************************************************************************/

Socket::Socket(Socket::Impl* impl)
    : pImpl{impl}
{}

bool Socket::isByteStream() const
{
    return pImpl->isByteStream();
}

Socket& Socket::setDelay(bool enable)
{
    pImpl->setDelay(enable);
    return *this;
}

bool Socket::getDelay() const
{
    return pImpl->getDelay();
}

SockAddr Socket::getAddr() const
{
    SockAddr sockAddr(pImpl->getAddr());
    LOG_DEBUG("%s", sockAddr.to_string().c_str());
    return sockAddr;
}

in_port_t Socket::getPort() const
{
    return pImpl->getPort();
}

SockAddr Socket::getPeerAddr() const
{
    return pImpl->getPeerAddr();
}

in_port_t Socket::getPeerPort() const
{
    return pImpl->getPeerPort();
}

size_t Socket::read(
        void* const  bytes,
        const size_t nbytes) const
{
    return pImpl->read(bytes, nbytes);
}

void Socket::write(
        const void* const bytes,
        const size_t      nbytes) const
{
    pImpl->write(bytes, nbytes);
}

void Socket::writev(
        const struct iovec* iov,
        const int           iovCnt)
{
    pImpl->writev(iov, iovCnt);
}

void Socket::shutdown() const
{
    pImpl->shutdown();
}

/******************************************************************************/

class ClntSock::Impl final : public Socket::Impl
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
        : Socket::Impl{sockAddr}
    {
        sockAddr.connect(sd);
    }
};

/******************************************************************************/

ClntSock::ClntSock(const SockAddr& sockAddr)
    : Socket(new Impl(sockAddr))
{}

/******************************************************************************/

class SrvrSock::Impl final : public Socket::Impl
{
public:
    /**
     * Constructs.
     *
     * @param[in] sockAddr           Socket address
     * @param[in] queueSize          Size of listening queue
     * @throws    std::system_error  Couldn't set SO_REUSEADDR on socket
     * @throws    std::system_error  Couldn't bind socket to `sockAddr`
     * @throws    std::system_error  Couldn't set SO_KEEPALIVE on socket
     * @throws    std::system_error  `::listen()` failure
     */
    Impl(   const SockAddr& sockAddr,
            const int       queueSize)
        : Socket::Impl(sockAddr)
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
                    + ", queueSize: " + std::to_string(queueSize));
    }

    /**
     * Accepts an incoming connection. Calls `::accept()`.
     *
     * @return                     The accepted socket
     * @throws  std::system_error  `::accept()` failure
     * @cancellationpoint
     */
    Socket::Impl* accept()
    {
        const int fd = ::accept(sd, nullptr, nullptr);

        if (fd == -1)
            throw SYSTEM_ERROR("accept() failure on socket " +
                    std::to_string(sd));

        return new Socket::Impl(fd);
    }
};

/******************************************************************************/

SrvrSock::SrvrSock(
        const SockAddr& sockAddr,
        const int       queueSize)
    : Socket{new Impl(sockAddr, queueSize)}
{}

Socket SrvrSock::accept() const
{
    return Socket{static_cast<SrvrSock::Impl*>(pImpl.get())->accept()};
}

} // namespace
