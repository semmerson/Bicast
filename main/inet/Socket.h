/**
 * BSD sockets.
 *
 *        File: Socket.h
 *  Created on: May 9, 2019
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MAIN_INET_SOCKET_H_
#define MAIN_INET_SOCKET_H_

#include "SockAddr.h"

#include <memory>
#include <string>

namespace hycast {

class Socket
{
public:
    class Impl;

protected:
    std::shared_ptr<Impl> pImpl;

    Socket(Impl* impl);

public:
    Socket() =default;

    virtual ~Socket() noexcept;

    /**
     * Indicates if this instance is valid (i.e., not default constructed).
     *
     * @return `true`   Instance is valid
     * @return `false`  Instance is not valid
     */
    operator bool() const noexcept;

    size_t hash() const noexcept;

    bool operator<(const Socket& rhs) const noexcept;

    void swap(Socket& socket) noexcept;

    std::string to_string() const;

    /**
     * Returns the local socket address.
     *
     * @return Local socket address
     */
    SockAddr getLclAddr() const;

    /**
     * Returns the local port number in host byte-order.
     *
     * @return             Local port number in host byte-order
     * @throw SystemError  Couldn't get name of local socket
     */
    in_port_t getLclPort() const;

    /**
     * Returns the remote socket address.
     *
     * @return Remote socket address
     */
    SockAddr getRmtAddr() const;

    /**
     * Returns the remote port number in host byte-order.
     *
     * @return Remote port number in host byte-order
     */
    in_port_t getRmtPort() const;

    bool write(const void*  data,
               const size_t nbytes) const;
    bool write(const bool value) const;
    bool write(const uint8_t value) const;
    bool write(const uint16_t value) const;
    bool write(const uint32_t value) const;
    bool write(const uint64_t value) const;
    bool write(const std::string& string) const;

    /**
     * Flushes the output.
     *
     * @retval `true`   Success but no guarantee that data was written
     * @retval `false`  Connection lost
     */
    bool flush();

    /**
     * Prepares the socket for further input.
     */
    void clear();

    bool read(void*        data,
              const size_t nbytes) const;
    bool read(bool& value) const;
    bool read(uint8_t& value) const;
    bool read(uint16_t& value) const;
    bool read(uint32_t& value) const;
    bool read(uint64_t& value) const;
    bool read(std::string& string) const;

    /**
     * Idempotent.
     */
    void shutdown(const int what = SHUT_RDWR) const;

    bool isShutdown() const;
};

/******************************************************************************/

class TcpSock : public Socket
{
public:
    class Impl;

protected:
    friend class TcpSrvrSock;

    TcpSock(Impl* impl);

public:
    TcpSock() =default;

    virtual ~TcpSock() noexcept;

    /**
     * If the following are all true:
     *   - The socket protocol is TCP or SCTP;
     *   - There's an outstanding packet acknowledgment; and
     *   - There's less than an MSS in the send buffer;
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
    TcpSock& setDelay(bool enable);
};

/******************************************************************************/

class TcpSrvrSock final : public TcpSock
{
public:
    class Impl;

    TcpSrvrSock() =default;

    /**
     * Constructs. Calls `::listen()`.
     *
     * @param[in] sockAddr           Socket address
     * @param[in] queueSize          Size of listening queue or `0` to obtain
     *                               the default.
     * @throws    std::system_error  Couldn't set SO_REUSEADDR on socket
     * @throws    std::system_error  Couldn't bind socket to `sockAddr`
     * @throws    std::system_error  Couldn't set SO_KEEPALIVE on socket
     */
    TcpSrvrSock(
            const SockAddr& sockaddr,
            const int       queueSize = 0);

    /**
     * Accepts an incoming connection. Calls `::accept()`.
     *
     * @return                     The accepted socket. Will test false if
     *                             `shutdown()` has been called.
     * @throws  std::system_error  `::accept()` failure
     * @cancellationpoint
     */
    TcpSock accept() const;
};

/******************************************************************************/

class TcpClntSock final : public TcpSock
{
public:
    class Impl;

    TcpClntSock() =default;

    /**
     * @cancellationpoint
     */
    TcpClntSock(const SockAddr& sockAddr);
};

/******************************************************************************/

class UdpSock final : public Socket
{
public:
    class Impl;

    static constexpr int MAX_PAYLOAD = 65507; ///< Maximum UDP payload in bytes

    UdpSock() =default;

    /**
     * Constructs a sending UDP socket.
     *
     * @cancellationpoint
     */
    UdpSock(const SockAddr& grpAddr);

    /**
     * Constructs a source-specific receiving socket.
     *
     * @cancellationpoint
     */
    UdpSock(const SockAddr& grpAddr,
            const InetAddr& rmtAddr);

    /**
     * Sets the interface to be used for multicasting.
     *
     * @param[in] iface  The interface
     * @return           This instance
     */
    const UdpSock& setMcastIface(const InetAddr& iface) const;

    /**
     * Flushes (writes) the UDP packet.
     *
     * @cancellationpoint  Yes
     */
    bool flush() const;

    /**
     * Clears the input buffer.
     */
    void clear() const;
};

} // namespace

#endif /* MAIN_INET_SOCKET_H_ */
