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

/// Socket
class Socket
{
public:
    class Impl;

protected:
    /// Smart pointer to the implementation
    std::shared_ptr<Impl> pImpl;

    /**
     * Constructs.
     * @param[in] impl  Pointer to an implementation
     */
    Socket(Impl* impl);

public:
    Socket() =default;

    virtual ~Socket() noexcept;

    /**
     * Indicates if this instance is valid (i.e., not default constructed).
     *
     * @return true     Instance is valid
     * @return false    Instance is not valid
     */
    operator bool() const noexcept;

    /**
     * Returns the hash code of this instance.
     * @return The hash code of this instance
     */
    size_t hash() const noexcept;

    /**
     * Indicates if this instance is less than another.
     * @param[in] rhs      The other, right-hand-side instance
     * @retval    true     This instance is less than the other
     * @retval    false    This instance is not less than the other
     */
    bool operator<(const Socket& rhs) const noexcept;

    /**
     * Swaps this instance with another.
     * @param[in,out] socket  The other instance
     */
    void swap(Socket& socket) noexcept;

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    std::string to_string() const;

    /**
     * Returns the socket descriptor.
     *
     * @return Socket descriptor
     */
    int getSockDesc() const;

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
    SockAddr getRmtAddr() const noexcept;

    /**
     * Returns the remote port number in host byte-order.
     *
     * @return Remote port number in host byte-order
     */
    in_port_t getRmtPort() const;

    /**
     * Assigns this instance a local socket address.
     *
     * @param[in] lclAddr   Local socket address. For multicast reception, this will be the socket
     *                      address of the multicast group.
     * @return              This instance
     * @throw  SystemError  System failure
     */
    Socket& bind(const SockAddr lclAddr);

    /**
     * Makes this instance non-blocking.
     *
     * @return  This instance.
     * @throw   SystemError  System failure
     */
    Socket& makeNonBlocking();

    /**
     * Makes this instance blocking.
     *
     * @return  This instance.
     * @throw   SystemError  System failure
     */
    Socket& makeBlocking();

    /**
     * Connects this instance to a remote socket address.
     *
     * @param[in]  rmtAddr   Remote socket address
     * @return               This instance
     * @throw   SystemError  System failure
     */
    Socket& connect(const SockAddr rmtAddr);

    /**
     * Writes bytes.
     * @param[in] data         Bytes to be written
     * @param[in] nbytes       Number of bytes to write
     * @retval    true         Success
     * @retval    false        Lost connection
     * @throw     SystemError  I/O failure
     */
    bool write(const void*  data,
               const size_t nbytes) const;
    /**
     * Writes a boolean.
     * @param[in] value        Value to be written
     * @retval    true         Success
     * @retval    false        Lost connection
     * @throw     SystemError  I/O failure
     */
    bool write(const bool value) const;
    /**
     * Writes an unsigned, 8-bit integer.
     * @param[in] value        Value to be written
     * @retval    true         Success
     * @retval    false        Lost connection
     * @throw     SystemError  I/O failure
     */
    bool write(const uint8_t value) const;
    /**
     * Writes an unsigned, 16-bit integer.
     * @param[in] value        Value to be written
     * @retval    true         Success
     * @retval    false        Lost connection
     * @throw     SystemError  I/O failure
     */
    bool write(const uint16_t value) const;
    /**
     * Writes an unsigned, 32-bit integer.
     * @param[in] value        Value to be written
     * @retval    true         Success
     * @retval    false        Lost connection
     * @throw     SystemError  I/O failure
     */
    bool write(const uint32_t value) const;
    /**
     * Writes an unsigned, 64-bit integer.
     * @param[in] value        Value to be written
     * @retval    true         Success
     * @retval    false        Lost connection
     * @throw     SystemError  I/O failure
     */
    bool write(const uint64_t value) const;

    /**
     * Flushes the output.
     *
     * @retval true     Success but no guarantee that data was written
     * @retval false    Connection lost
     */
    bool flush();

    /**
     * Prepares the socket for further input.
     */
    void clear();

    /**
     * Reads bytes.
     * @param[out] data         Buffer to be set
     * @param[in]  nbytes       Number of bytes to read
     * @retval     true         Success
     * @retval     false        Lost connection
     * @throw      SystemError  I/O failure
     */
    bool read(void*        data,
              const size_t nbytes) const;
    /**
     * Reads a boolean value.
     * @param[out] value        Value to be set
     * @retval     true         Success
     * @retval     false        Lost connection
     * @throw      SystemError  I/O failure
     */
    bool read(bool&     value) const;
    /**
     * Reads an unsigned, 8-bit value.
     * @param[out] value        Value to be set
     * @retval     true         Success
     * @retval     false        Lost connection
     * @throw      SystemError  I/O failure
     */
    bool read(uint8_t&  value) const;
    /**
     * Reads an unsigned, 16-bit value.
     * @param[out] value        Value to be set
     * @retval     true         Success
     * @retval     false        Lost connection
     * @throw      SystemError  I/O failure
     */
    bool read(uint16_t& value) const;
    /**
     * Reads an unsigned, 32-bit value.
     * @param[out] value        Value to be set
     * @retval     true         Success
     * @retval     false        Lost connection
     * @throw      SystemError  I/O failure
     */
    bool read(uint32_t& value) const;
    /**
     * Reads an unsigned, 64-bit value.
     * @param[out] value        Value to be set
     * @retval     true         Success
     * @retval     false        Lost connection
     * @throw      SystemError  I/O failure
     */
    bool read(uint64_t& value) const;
    /**
     * Reads a string.
     * @param[out] string       String to be set
     * @retval     true         Success
     * @retval     false        Lost connection
     * @throw      SystemError  I/O failure
     */
    template<typename UINT>
    bool read(std::string& string) const;

    /**
     * Idempotent.
     */
    void shutdown(const int what = SHUT_RDWR) const;

    /**
     * Indictes if the connection is shut down.
     * @retval true     The connection is shut down
     * @retval false    The connection is not shut down
     */
    bool isShutdown() const;
};

/******************************************************************************/

/// A TCP socket
class TcpSock : public Socket
{
public:
    class Impl;

protected:
    friend class TcpSrvrSock;

    /**
     * Constructs.
     * @param[in] impl  Pointer to an implementation
     */
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

/// A TCP server socket
class TcpSrvrSock final : public TcpSock
{
public:
    class Impl;

    TcpSrvrSock() =default;

    /**
     * Constructs. Starts listening on the created socket.
     *
     * @param[in] sockAddr           Socket address
     * @param[in] queueSize          Size of listening queue or `0` to obtain
     *                               the default.
     * @throws    std::system_error  Couldn't set SO_REUSEADDR on socket
     * @throws    std::system_error  Couldn't bind socket to `sockAddr`
     * @throws    std::system_error  Couldn't set SO_KEEPALIVE on socket
     */
    explicit TcpSrvrSock(const SockAddr sockAddr,
                         const int      queueSize = 0);

    /**
     * Accepts the next, incoming connection.
     *
     * @return               The accepted socket. Will test false if `shutdown()` has been called.
     * @throws  SystemError  Couldn't accept a connection on the socket
     * @cancellationpoint
     */
    TcpSock accept() const;
};

/******************************************************************************/

/// A client-side TCP socket
class TcpClntSock final : public TcpSock
{
public:
    class Impl;

    TcpClntSock() =default;

    /**
     * Constructs an unbound socket of a given address family.
     *
     * @param[in] family  Address family. E.g., `AF_INET`, `AF_INET6`
     */
    TcpClntSock(int family);

    /**
     * @param[in] srvrAddr         Socket address of server
     * @param[in] timeout          Timeout in ms. <0 => system's default timeout.
     * @throw     LogicError       Destination port number is zero
     * @throw     SystemError      Couldn't connect to `sockAddr`. Bad failure.
     * @throw     RuntimeError     Connection attempt timed-out
     * @cancellationpoint
     * @see `makeBlocking()`
     */
    TcpClntSock(
            const SockAddr srvrAddr,
            const int      timeout);

    /**
     * @param[in] srvrAddr         Socket address of server
     * @throw     LogicError       Destination port number is zero
     * @throw     SystemError      Couldn't connect to `sockAddr`. Bad failure.
     * @throw     RuntimeError     Connection attempt timed-out
     * @cancellationpoint
     * @see `makeBlocking()`
     */
    explicit TcpClntSock(const SockAddr srvrAddr);
};

/******************************************************************************/

/// A UDP socket
class UdpSock final : public Socket
{
public:
    class Impl;

    static constexpr int MAX_PAYLOAD = 65507; ///< Maximum UDP payload in bytes

    UdpSock() =default;

    /**
     * Constructs a sending UDP socket. The operating system will choose the interface.
     *
     * @param[in] destAddr   Destination socket address
     * @cancellationpoint
     */
    UdpSock(const SockAddr destAddr);

    /**
     * Constructs a sending UDP socket.
     *
     * @param[in] destAddr   Destination socket address
     * @param[in] ifaceAddr  IP address of interface to use. If wildcard, then O/S chooses.
     * @cancellationpoint
     */
    UdpSock(
            const SockAddr destAddr,
            const InetAddr ifaceAddr);

    /**
     * Constructs a source-specific multicast, receiving socket.
     *
     * @param[in] ssmAddr          IP address of source-specific multicast group
     * @param[in] srcAddr          IP address of source
     * @param[in] iface            IP address of interface to use. If wildcard, then O/S chooses.
     * @throw     InvalidArgument  Multicast group IP address isn't source-specific
     * @throw     LogicError       IP address families don't match
     * @cancellationpoint
     */
    UdpSock(const SockAddr ssmAddr,
            const InetAddr srcAddr,
            const InetAddr iface);

    /**
     * Sets the interface to be used for multicasting.
     *
     * @param[in] iface  The interface
     * @return           This instance
     */
    const UdpSock& setMcastIface(const InetAddr iface) const;

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
