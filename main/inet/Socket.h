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

    virtual ~Socket() noexcept =0;

    operator bool() const noexcept
    {
        return (bool)pImpl;
    }

    /**
     * Returns the local socket address.
     *
     * @return Local socket address
     */
    SockAddr getLclAddr() const;

    /**
     * Returns the local port number in host byte-order.
     *
     * @return Local port number in host byte-order
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

    /**
     * Idempotent.
     */
    void shutdown() const;

    bool isShutdown() const;
};

/******************************************************************************/

class InetSock : public Socket
{
public:
    class Impl;

protected:
    InetSock(Impl* impl);

public:
    class Impl;

    InetSock() =default;

    virtual ~InetSock() noexcept =0;

    operator bool() noexcept;

    static inline uint8_t hton(const uint8_t value)
    {
        return value;
    }

    static inline uint16_t hton(const uint16_t value)
    {
        return htons(value);
    }

    static inline uint32_t hton(const uint32_t value)
    {
        return htonl(value);
    }

    static inline int32_t hton(const int32_t value)
    {
        return htonl(value);
    }

    static inline uint64_t hton(uint64_t value)
    {
        uint64_t  v64;
        uint32_t* v32 = reinterpret_cast<uint32_t*>(&v64);

        v32[0] = hton(static_cast<uint32_t>(value >> 32));
        v32[1] = hton(static_cast<uint32_t>(value));

        return v64;
    }

    static inline uint16_t ntoh(const uint16_t value)
    {
        return ntohs(value);
    }

    static inline uint32_t ntoh(const uint32_t value)
    {
        return ntohl(value);
    }

    static inline uint64_t ntoh(uint64_t value)
    {
        uint32_t* v32 = reinterpret_cast<uint32_t*>(&value);

        return (static_cast<uint64_t>(ntoh(v32[0])) << 32) | ntoh(v32[1]);
    }
};

/******************************************************************************/

class TcpSock : public InetSock
{
public:
    class Impl;

protected:
    friend class TcpSrvrSock;

    TcpSock(Impl* impl);

public:
    TcpSock() =default;

    virtual ~TcpSock() noexcept;

    virtual std::string to_string() const;

    /**
     * If the socket protocol is TCP or SCTP, there's an outstanding packet
     * acknowledgment, and there's less than an MSS in the send buffer,
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

    /**
     * Writes bytes to the socket. No host-to-network translation is performed.
     *
     * @param[in] bytes        Data to be written
     * @param[in] nbytes       Number of bytes
     * @throws    EofError     Socket is closed
     * @throws    SystemError  System error
     */
    void write(
            const void* bytes,
            size_t      nbytes) const;

    /**
     * Writes a value to the socket. No host-to-network translation is
     * performed.
     *
     * @param[in] value        Value to be written
     * @throws    EofError     Socket is closed
     * @throws    SystemError  System error
     */
    void write(bool value) const;

    /**
     * Writes a value to the socket. No host-to-network translation is
     * performed.
     *
     * @param[in] value        Value to be written
     * @throws    EofError     Socket is closed
     * @throws    SystemError  System error
     */
    void write(uint8_t value) const;

    /**
     * Writes a value to the socket. Host-to-network translation is performed.
     *
     * @param[in] value        Value to be written
     * @throws    EofError     Socket is closed
     * @throws    SystemError  System error
     */
    void write(uint16_t value) const;

    /**
     * Writes a value to the socket. Host-to-network translation is performed.
     *
     * @param[in] value        Value to be written
     * @throws    EofError     Socket is closed
     * @throws    SystemError  System error
     */
    void write(uint32_t value) const;

    /**
     * Writes a value to the socket. Host-to-network translation is performed.
     *
     * @param[in] value        Value to be written
     * @throws    EofError     Socket is closed
     * @throws    SystemError  System error
     */
    void write(uint64_t value) const;

    /**
     * Reads bytes from the socket. No network-to-host translation is performed.
     *
     * @param[out] bytes         Buffer into which data will be read
     * @param[in]  nbytes        Maximum mount of data to read in bytes
     * @retval     `true`        Success
     * @retval     `false`       EOF or `shutdown()` called
     * @throws     SystemError   Read error
     */
    bool read(
            void*        bytes,
            const size_t nbytes) const;

    /**
     * Reads a value from the socket. No network-to-host translation is
     * performed.
     *
     * @param[out] value         Destination for value
     * @retval     `true`        Success
     * @retval     `false`       EOF or `shutdown()` called
     * @throws     SystemError   Read error
     */
    bool read(bool&     value) const;

    /**
     * Reads a value from the socket. No network-to-host translation is
     * performed.
     *
     * @param[out] value         Destination for value
     * @retval     `true`        Success
     * @retval     `false`       EOF or `shutdown()` called
     * @throws     SystemError   Read error
     */
    bool read(uint8_t&  value) const;

    /**
     * Reads a value from the socket. Network-to-host translation is performed.
     *
     * @param[out] value         Destination for value
     * @retval     `true`        Success
     * @retval     `false`       EOF or `shutdown()` called
     * @throws     SystemError   Read error
     */
    bool read(uint16_t& value) const;

    /**
     * Reads a value from the socket. Network-to-host translation is performed.
     *
     * @param[out] value         Destination for value
     * @retval     `true`        Success
     * @retval     `false`       EOF or `shutdown()` called
     * @throws     SystemError   Read error
     */
    bool read(uint32_t& value) const;

    /**
     * Reads a value from the socket. Network-to-host translation is performed.
     *
     * @param[out] value         Destination for value
     * @retval     `true`        Success
     * @retval     `false`       EOF or `shutdown()` called
     * @throws     SystemError   Read error
     */
    bool read(uint64_t& value) const;
};

/******************************************************************************/

class TcpSrvrSock final : public TcpSock
{
    class Impl;

public:
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

    std::string to_string() const;

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
    class Impl;

public:
    TcpClntSock() =default;

    /**
     * @cancellationpoint
     */
    TcpClntSock(const SockAddr& sockAddr);
};

/******************************************************************************/

class UdpSock final : public InetSock
{
    class Impl;

public:
    static const int MAX_PAYLOAD = 1472;

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

    std::string to_string() const;

    /**
     * Adds bytes to be written. No host-to-network translation is performed.
     *
     * @param[in] data    Bytes to be added. <b>Must exist until `send()`
     *                    returns.</b>
     * @param[in] nbytes  Number of bytes to be added.
     */
    void addWrite(
            const void*  data,
            const size_t nbytes);

    /**
     * Adds a byte. Host-to-network translation is performed.
     *
     * @param[in] value  Value to be added.
     */
    void addWrite(const uint8_t value);

    /**
     * Adds a value. Host-to-network translation is performed.
     *
     * @param[in] value  Value to be added.
     */
    void addWrite(const bool value);

    /**
     * Adds a value. Host-to-network translation is performed.
     *
     * @param[in] value  Value to be added.
     */
    void addWrite(const uint16_t value);

    /**
     * Adds a value. Host-to-network translation is performed.
     *
     * @param[in] value  Value to be added.
     */
    void addWrite(const uint32_t value);

    /**
     * Adds a value. Host-to-network translation is performed.
     *
     * @param[in] value  Value to be added.
     */
    void addWrite(const uint64_t value);

    /**
     * Writes the UDP packet.
     *
     * @cancellationpoint  Yes
     */
    void write();

    /**
     * Adds bytes to be peeked by the next call to `peek()` Previously peeked
     * bytes are skipped. No network-to-host translation is performed.
     *
     * @param[out] data             Destination for peeked bytes. <b>Must exist
     *                              until `peek()` returns.</b>
     * @param[in]  nbytes           Number of bytes to be peeked
     * @throws     InvalidArgument  Addition would exceed UDP packet size
     * @throws     LogicError       Out of vector I/O elements
     * @cancellationpoint           No
     */
    void addPeek(
            void* const  data,
            const size_t nbytes);

    /**
     * Adds a value to be peeked by the next call to `peek()` Previously peeked
     * bytes are skipped. Network-to-host translation is performed.
     *
     * @param[out] value            Destination for peeked value. <b>Must exist
     *                              until `peek()` returns.</b>
     * @throws     InvalidArgument  Addition would exceed UDP packet size
     * @throws     LogicError       Out of vector I/O elements
     * @cancellationpoint           No
     */
    void addPeek(uint8_t& value);

    /**
     * Adds a value to be peeked by the next call to `peek()` Previously peeked
     * bytes are skipped. Network-to-host translation is performed.
     *
     * @param[out] value            Destination for peeked value. <b>Must exist
     *                              until `peek()` returns.</b>
     * @throws     InvalidArgument  Addition would exceed UDP packet size
     * @throws     LogicError       Out of vector I/O elements
     * @cancellationpoint           No
     */
    void addPeek(uint16_t& value);

    /**
     * Adds a value to be peeked by the next call to `peek()` Previously peeked
     * bytes are skipped. Network-to-host translation is performed.
     *
     * @param[out] value            Destination for peeked value. <b>Must exist
     *                              until `peek()` returns.</b>
     * @throws     InvalidArgument  Addition would exceed UDP packet size
     * @throws     LogicError       Out of vector I/O elements
     * @cancellationpoint           No
     */
    void addPeek(uint32_t& value);

    /**
     * Adds a value to be peeked by the next call to `peek()` Previously peeked
     * bytes are skipped. Network-to-host translation is performed.
     *
     * @param[out] value            Destination for peeked value. <b>Must exist
     *                              until `peek()` returns.</b>
     * @throws     InvalidArgument  Addition would exceed UDP packet size
     * @throws     LogicError       Out of vector I/O elements
     * @cancellationpoint           No
     */
    void addPeek(uint64_t& value);

    /**
     * Peeks at the UDP packet using the I/O vector set by previous calls to
     * `setPeek()`. Previously peeked bytes are skipped.
     *
     * @retval    `false`       EOF or `halt()` called
     * @retval    `true`        Success
     * @throws    SystemError   I/O error
     * @throws    RuntimeError  Packet is too small
     * @cancellationpoint       Yes
     */
    bool peek() const;

    /**
     * Discards the current packet. Idempotent.
     */
    void discard();
};

} // namespace

#endif /* MAIN_INET_SOCKET_H_ */
