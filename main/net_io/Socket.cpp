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

    explicit Impl(const SockAddr& sockAddr)
        : sd{-1}
    {
        struct sockaddr sockaddr;
        socklen_t       socklen;

        sockAddr.getSockAddr(sockaddr, socklen);

        sd = ::socket(sockaddr.sa_family, SOCK_STREAM, 0);

        if (sd == -1)
            throw SYSTEM_ERROR("Couldn't create streaming socket for address " +
                    sockAddr.to_string());
    }

    static const SockAddr getSockAddr(struct sockaddr& sockaddr)
    {
        SockAddr peerAddr;

        if (sockaddr.sa_family == AF_INET) {
            struct sockaddr_in* addr =
                    reinterpret_cast<struct sockaddr_in*>(&sockaddr);

            peerAddr = SockAddr(*addr);
        }
        else if (sockaddr.sa_family == AF_INET6) {
            struct sockaddr_in6* addr =
                    reinterpret_cast<struct sockaddr_in6*>(&sockaddr);

            peerAddr = SockAddr(*addr);

        }
        else {
            throw RUNTIME_ERROR("Unsupported address family: " +
                    std::to_string(sockaddr.sa_family));
        }

        return peerAddr;
    }

public:
    explicit Impl(const int sd) noexcept
        : sd{sd}
    {}

    virtual ~Impl() noexcept
    {
        ::close(sd);
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

        return getSockAddr(sockaddr);
    }

    const SockAddr getPeerAddr() const
    {
        struct sockaddr sockaddr = {};
        socklen_t       socklen = sizeof(sockaddr);

        if (::getpeername(sd, &sockaddr, &socklen))
            throw SYSTEM_ERROR("getpeername() failure on socket " +
                    std::to_string(sd));

        return getSockAddr(sockaddr);
    }

    in_port_t getPort() const noexcept
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
        ssize_t status = ::read(sd, data, nbytes);

        if (status < 0)
            throw SYSTEM_ERROR("read() failure on socket " +
                    std::to_string(sd));

        return status;
    }

    void write(
            const void* data,
            size_t      nbytes) const
    {
        if (::write(sd, data, nbytes) != nbytes)
            throw SYSTEM_ERROR("Couldn't write " + std::to_string(nbytes) +
                    " bytes to socket " + std::to_string(sd));
    }

    void writev(
            const struct iovec* iov,
            const int           iovCnt)
    {
        if (::writev(sd, iov, iovCnt) == -1)
            throw SYSTEM_ERROR("Couldn't write " + std::to_string(iovCnt) +
                    "-element vector to socket " + std::to_string(sd));
    }
};

/******************************************************************************/

Socket::Socket(Socket::Impl* impl)
    : pImpl{impl}
{}

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
    return pImpl->getAddr();
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

/******************************************************************************/

class ClntSock::Impl final : public Socket::Impl
{
public:
    /**
     * Constructs.
     *
     * @param[in] sockAddr       Address of remote endpoint
     * @throw std::system_error  Couldn't connect to `sockAddr`
     */
    Impl(const SockAddr& sockAddr)
        : Socket::Impl{sockAddr}
    {
        struct sockaddr sockaddr;
        socklen_t       socklen;

        sockAddr.getSockAddr(sockaddr, socklen);

        if (::connect(sd, &sockaddr, socklen))
            throw SYSTEM_ERROR("Couldn't connect socket " + std::to_string(sd) +
                    " to " + sockAddr.to_string());
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
     * @throws    std::system_error  Couldn't set SO_REUSEADDR on socket
     * @throws    std::system_error  Couldn't bind socket to `sockAddr`
     * @throws    std::system_error  Couldn't set SO_KEEPALIVE on socket
     */
    Impl(const SockAddr& sockAddr)
        : Socket::Impl{sockAddr}
    {
        const int enable = 1;
        if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)))
            throw SYSTEM_ERROR("Couldn't set SO_REUSEADDR on socket " +
                    std::to_string(sd));

        struct sockaddr sockaddr;
        socklen_t       socklen;

        sockAddr.getSockAddr(sockaddr, socklen);

        if (::bind(sd, &sockaddr, socklen))
            throw SYSTEM_ERROR("Couldn't bind socket " + std::to_string(sd) +
                    " to " + sockAddr.to_string());

        if (setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)))
            throw SYSTEM_ERROR("Couldn't set SO_KEEPALIVE on socket " +
                    std::to_string(sd));
    }

    /**
     * Calls `::listen()` on the socket.
     *
     * @param[in] queueSize          Size of the listening queue
     * @throws    std::system_error  `::listen()` failure
     */
    void listen(const int queueSize) const
    {
        if (::listen(sd, queueSize))
            throw SYSTEM_ERROR("listen() failure: {sock: " + std::to_string(sd)
                    + ", queueSize: " + std::to_string(queueSize));
    }

    /**
     * Accepts an incoming connection. Calls `::accept()`.
     *
     * @return                     The accepted socket
     * @throws  std::system_error  `::accept()` failure
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

SrvrSock::SrvrSock(const SockAddr& sockAddr)
    : Socket{new Impl(sockAddr)}
{}

void SrvrSock::listen(const int queueSize) const
{
    static_cast<SrvrSock::Impl*>(pImpl.get())->listen(queueSize);
}

Socket SrvrSock::accept() const
{
    return Socket{static_cast<SrvrSock::Impl*>(pImpl.get())->accept()};
}

} // namespace
