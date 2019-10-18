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

    Socket(Impl* impl); // Should be protected but won't compile if so

public:
    Socket() =default;

    virtual ~Socket() noexcept =default;

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

    static inline uint16_t hton(const uint16_t value)
    {
        return htons(value);
    }

    static inline uint32_t hton(const uint32_t value)
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
    TcpSock& setDelay(bool enable);

    void write(
            const void* bytes,
            size_t      nbytes) const;

    void write(uint16_t value) const;
    void write(uint32_t value) const;
    void write(uint64_t value) const;

    /**
     * Reads from the socket.
     *
     * @param[out] bytes         Buffer into which data will be read
     * @param[in]  nbytes        Maximum amount of data to read in bytes
     * @return                   Number of bytes actually read. 0 => EOF
     * @throws     SystemError   Read error
     */
    size_t read(
            void*        bytes,
            const size_t nbytes) const;

    bool read(uint16_t& value) const;
    bool read(uint32_t& value) const;
    bool read(uint64_t& value) const;

    void shutdown() const;
};

/******************************************************************************/

class TcpSrvrSock final : public TcpSock
{
    class Impl;

public:
    TcpSrvrSock() =default;

    /**
     * Constructs.
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
     * @return                     The accepted socket
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

class UdpSndrSock final : public InetSock
{
    class Impl;

public:
    UdpSndrSock() =default;

    /**
     * @cancellationpoint
     */
    UdpSndrSock(const SockAddr& grpAddr);

    void write(
            const struct iovec* iov,
            const int           iovCnt);
};

/******************************************************************************/

class UdpRcvrSock final : public InetSock
{
    class Impl;

public:
    UdpRcvrSock() =default;

    /**
     * @cancellationpoint
     */
    UdpRcvrSock(
            const SockAddr& grpAddr,
            const InetAddr& srcAddr);

    /**
     * Peeks at at UDP record.
     *
     * @param[out] bytes        Buffer
     * @param[in]  nbytes       Maximum number of bytes to read into `bytes`
     * @return                  Number of bytes read. 0 => EOF.
     * @throws     SystemError  Error reading
     */
    size_t peek(
            void*  bytes,
            size_t nbytes);

    /**
     * Reads a UDP record.
     *
     * @param[in] iov           I/O vector
     * @return                  Number of bytes read. 0 => EOF.
     * @throws     SystemError  Error reading
     */
    size_t read(
            const struct iovec* iov,
            const int           iovCnt);
};

} // namespace

#endif /* MAIN_NET_IO_SOCKET_H_ */
