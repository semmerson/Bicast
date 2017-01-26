/**
 * This file declares a handle classes for UDP sockets.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYIING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Socket.h
 * @author: Steven R. Emmerson
 */

#ifndef UDPSOCK_H_
#define UDPSOCK_H_

#include "InetAddr.h"
#include "InetSockAddr.h"

#include <sys/types.h>

namespace hycast {

/**
 * Abstract base class for a UDP socket. Such a socket is bound to a local
 * address and can receive UDP packets.
 */
class UdpSock
{
protected:
    class Impl; // Forward declaration of implementation

    std::shared_ptr<Impl> pImpl;

    /**
     * Constructs from a pointer to an implementation.
     * @param[in] pImpl  Pointer to implementation
     */
    UdpSock(Impl* const pImpl);

public:
    /**
     * Destroys.
     */
    virtual ~UdpSock() {};

    /**
     * Allows multiple sockets to use the same port number for incoming
     * packets
     */
    void shareLocalPort() const;

    /**
     * Returns a string representation of this instance's socket.
     * @return String representation of this instance's socket
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    virtual std::string to_string() const;

    /**
     * Peeks at the contents of the current UDP packet. Waits for a packet if
     * necessary. The packet is left in the input buffer.
     * @param[in] buf  Buffer into which to copy the first bytes of the packet
     * @param[in] len  Number of bytes to copy
     * @retval    0    Socket is closed
     * @return         Number of bytes copied
     * @throws std::system_error  I/O error
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    ssize_t peek(
            void*  buf,
            size_t len) const;

    /**
     * Returns the size, in bytes, of the current message. Waits for the
     * message if necessary. The message is left in the socket's input buffer.
     * @returns Size of message in bytes. Will equal 0 when socket is closed
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     */
    uint32_t getSize() const;

    /**
     * Receives a message.
     * @param[out] msg     Receive buffer
     * @param[in]  len     Size of receive buffer in bytes
     * @param[in]  flags   Type of message reception. Logical OR of zero or
     *                     more of
     *                       - `MSG_OOB`  Requests out-of-band data
     *                       - `MSG_PEEK` Peeks at the incoming message
     * @throws std::system_error on I/O failure or if number of bytes read
     *     doesn't equal `len`
     */
    void recv(
            void*        msg,
            const size_t len,
            const int    flags = 0) const;

    /**
     * Receives a message.
     * @param[in] iovec     Vector comprising message to receive
     * @param[in] iovcnt    Number of elements in `iovec`
     * @param[in] flags     Type of message reception. Logical OR of zero or
     *                      more of
     *                      - `MSG_OOB`  Requests out-of-band data
     *                      - `MSG_PEEK` Peeks at the incoming message
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety Safe
     */
    void recvv(
            struct iovec*  iovec,
            const int      iovcnt,
            const int      flags = 0) const;

    /**
     * Indicates if this instance has a current message.
     * @retval true   Yes
     * @retval false  No
     */
    bool hasMessage() const;

    /**
     * Discards the current message.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Thread-compatible but not thread-safe
     */
    void discard() const;

    /**
     * Closes the underlying socket.
     * @exceptionsafety Nothrow
     * @threadsafety    Compatible but not safe
     */
    void close() const;
};

/**
 * Server UDP socket. The remote endpoint of such a socket is unbound. The
 * server listens to the local endpoint.
 */
class SrvrUdpSock final : public UdpSock
{
protected:
    class Impl;

private:
    inline Impl* getPimpl() const noexcept
    {
        return reinterpret_cast<Impl*>(pImpl.get());
    }

public:
    /**
     * Default constructs.
     */
    SrvrUdpSock() =default;

    /**
     * Constructs an instance that listens to a given address.
     * @param[in] srvrAddr  Internet address for the server
     */
    SrvrUdpSock(const InetSockAddr& srvrAddr);

    /**
     * Returns a string representation of this instance's socket.
     * @return String representation of this instance's socket
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    std::string to_string() const;
};

/**
 * Client UDP socket. The remote endpoint of such a socket is bound to a
 * specific address.
 */
class ClntUdpSock : public UdpSock
{
protected:
    class Impl;

    ClntUdpSock(Impl* const pImpl);

private:
    inline Impl* getPimpl() const noexcept
    {
        return reinterpret_cast<Impl*>(pImpl.get());
    }

public:
    /**
     * Constructs an instance with unbound endpoints.
     */
    ClntUdpSock();

    /**
     * Constructs.
     * @param[in] mcastAddr  Internet address of the multicast group
     */
    ClntUdpSock(const InetSockAddr& mcastAddr);

    /**
     * Returns a string representation of this instance's socket.
     * @return String representation of this instance's socket
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    std::string to_string() const;

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
    const ClntUdpSock& setHopLimit(
            const unsigned limit) const;

    /**
     * Sends a message.
     * @param[in] msg       Message to be sent
     * @param[in] len       Size of message in bytes
     * @throws std::system_error if an I/O error occurred
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void send(
            const void*    msg,
            const size_t   len) const;

    /**
     * Sends a message.
     * @param[in] iovec     Vector comprising message to send
     * @param[in] iovcnt    Number of elements in `iovec`
     */
    void sendv(
            struct iovec*  iovec,
            const int      iovcnt) const;
};

/**
 * Multicast UDP socket. The remote endpoint of such a socket is bound to a
 * multicast group address.
 */
class McastUdpSock final : public ClntUdpSock
{
protected:
    class Impl;

private:
    inline Impl* getPimpl() const noexcept
    {
        return reinterpret_cast<Impl*>(pImpl.get());
    }

public:
    /**
     * Constructs an instance with unbound endpoints.
     */
    McastUdpSock() =default;

    /**
     * Constructs an instance whose remote endpoint is bound to the given
     * multicast group and whose local endpoint is chosen by the system. The
     * socket will accept any packet sent to the multicast group from any
     * source.
     * @param[in] mcastAddr  Address of multicast group
     */
    McastUdpSock(const InetSockAddr& mcastAddr);

    /**
     * Constructs a source-specific instance. The local and remote endpoints
     * will be bound to the given multicast group and only packets from the
     * source address will be accepted.
     * @param[in] mcastAddr   Address of multicast group
     * @param[in] sourceAddr  Address of source
     */
    McastUdpSock(
            const InetSockAddr& mcastAddr,
            const InetAddr&     sourceAddr);

    /**
     * Returns a string representation of this instance's socket.
     * @return String representation of this instance's socket
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    std::string to_string() const;

    /**
     * Sets whether or not a multicast packet sent to a socket will also be
     * read from the same socket. Such looping in enabled by default.
     * @param[in] enable  Whether or not to enable reception of sent packets
     * @return  This instance
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    const McastUdpSock& setMcastLoop(
            const bool enable) const;
};

} // namespace

#endif
