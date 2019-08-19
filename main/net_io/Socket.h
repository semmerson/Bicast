/**
 * BSD sockets.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Socket.h
 *  Created on: May 9, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_NET_IO_SOCKET_H_
#define MAIN_NET_IO_SOCKET_H_

#include "SockAddr.h"

#include <memory>

namespace hycast {

class Socket
{
public:
    class Impl;

protected:
    std::shared_ptr<Impl> pImpl;

    Socket() =default;

public:
    Socket(Impl* impl); // Should be protected but won't compile if so

    virtual ~Socket() noexcept =default;

    /**
     * If the socket protocol is TCP or SCTP, the previous sent packet hasn't
     * yet been acknowledged, and there's less than an MSS in the send buffer,
     * then this function sets whether or not the protocol layer will wait for
     * the outstanding acknowledgment before sending the sub-MSS packet. This is
     * the Nagle algorithm.
     *
     * @param[in] enable             Whether or not to delay sending the sub-MSS
     *                               packet until an ACK of the previous packet
     *                               is received
     * @return                       Reference to this instance
     * @throws    std::system_error  `setsockopt()` failure
     */
    Socket& setDelay(bool enable);

    /**
     * Returns whether or not the Nagle algorithm is enabled.
     *
     * @retval `true`             The Nagle algorithm is enabled
     * @retval `false`            The Nagle algorithm is not enabled
     * @throws std::system_error  `getsockopt()` failure
     */
    bool getDelay() const;

    /**
     * Returns the local socket address.
     *
     * @return Local socket address
     */
    SockAddr getAddr() const;

    /**
     * Returns the local port number in host byte-order.
     *
     * @return Local port number in host byte-order
     */
    in_port_t getPort() const;

    /**
     * Returns the remote socket address.
     *
     * @return Remote socket address
     */
    SockAddr getPeerAddr() const;

    /**
     * Returns the remote port number in host byte-order.
     *
     * @return Remote port number in host byte-order
     */
    in_port_t getPeerPort() const;

    /**
     * Reads from the socket.
     *
     * @param[in] nbytes         Amount of data to read in bytes
     * @retval    0              EOF
     * @return                   The number of bytes actually read
     * @throw std::system_error  I/O failure
     */
    size_t read(
            void*  data,
            size_t nbytes) const;

    void write(
            const void* data,
            size_t      nbytes) const;

    void writev(
            const struct iovec* iov,
            const int           iovCnt);
};

/******************************************************************************/

class ClntSock final : public Socket
{
    class Impl;

public:
    ClntSock() =default;

    ClntSock(const SockAddr& sockAddr);
};

/******************************************************************************/

class SrvrSock : public Socket
{
    class Impl;

public:
    SrvrSock() =default;

    /**
     * Constructs.
     *
     * @param[in] sockAddr           Socket address
     * @throws    std::system_error  Couldn't set SO_REUSEADDR on socket
     * @throws    std::system_error  Couldn't bind socket to `sockAddr`
     * @throws    std::system_error  Couldn't set SO_KEEPALIVE on socket
     */
    SrvrSock(const SockAddr& sockaddr);

    /**
     * Calls `::listen()` on the socket.
     *
     * @param[in] queueSize          Size of the listening queue
     * @throws    std::system_error  `::listen()` failure
     */
    void listen(int queueSize = 0) const;

    /**
     * Accepts an incoming connection. Calls `::accept()`.
     *
     * @return                     The accepted socket
     * @throws  std::system_error  `::accept()` failure
     */
    Socket accept() const;
};

} // namespace

#endif /* MAIN_NET_IO_SOCKET_H_ */
