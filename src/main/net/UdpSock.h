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
 * Abstract base class for a UDP socket.
 */
class UdpSock
{
protected:
    class Impl; // Forward declaration of implementation

    std::shared_ptr<Impl> pImpl;

    /**
     * Default constructs.
     */
    UdpSock() =default;

    /**
     * Constructs from a pointer to an implementation.
     * @param[in] pImpl  Pointer to implementation.
     */
    UdpSock(Impl* const pImpl);

public:
    /**
     * Destroys.
     */
    virtual ~UdpSock();

    /**
     * Returns the local socket address.
     * @return The local socket address
     * @threadsafety     Safe
     */
    virtual const InetSockAddr getLocalAddr() const =0;

    /**
     * Returns a string representation of this instance's socket.
     * @return String representation of this instance's socket
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    virtual std::string to_string() const =0;
};

/**
 * Input UDP socket. The local endpoint is bound to a specific address.
 */
class InUdpSock : public UdpSock
{
protected:
    class Impl;

    /**
     * Constructs from a pointer to an implementation.
     * @param[in] pImpl  Pointer to implementation.
     */
    InUdpSock(Impl* const pImpl);

private:
    Impl* getPimpl() const noexcept;

public:
    /**
     * Default constructs.
     */
    InUdpSock() =default;

    /**
     * Constructs an instance that listens to a given address.
     * @param[in] localAddr  Local Internet address to listen to
     * @param[in] sharePort  Whether or not the local port number should be
     *                       shared amongst multiple sockets
     */
    explicit InUdpSock(const InetSockAddr& localAddr, bool sharePort = false);

    /**
     * Destroys.
     */
    virtual ~InUdpSock();

    /**
     * Returns the local socket address.
     * @return The local socket address
     * @exceptionsafety  No throw
     * @threadsafety     Safe
     */
    const InetSockAddr getLocalAddr() const noexcept;

    /**
     * Returns a string representation of this instance's socket.
     * @return String representation of this instance's socket
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    virtual std::string to_string() const;

    /**
     * Scatter-receives a record. Waits for the record if necessary. If the
     * requested number of bytes to be read is less than the record size, then
     * the excess bytes are discarded.
     * @param[in] iovec   Scatter-read vector
     * @param[in] iovcnt  Number of elements in scatter-read vector
     * @param[in] peek    Whether or not to peek at the record. The data is
     *                    treated as unread and the next recv() or similar
     *                    function shall still return this data.
     * @retval    0       Stream is closed
     * @return            Actual number of bytes read into the buffers.
     */
    virtual size_t recv(
            struct iovec* iovec,
            const int     iovcnt,
            const bool    peek = false);

    /**
     * Receives a record. Waits for the record if necessary. If the requested
     * number of bytes to be read is less than the record size, then the excess
     * bytes are discarded.
     * @param[in] buf     Receive buffer
     * @param[in] len     Size of receive buffer in bytes
     * @param[in] peek    Whether or not to peek at the record. The data is
     *                    treated as unread and the next recv() or similar
     *                    function shall still return this data.
     * @retval    0       Socket is closed
     * @return            Actual number of bytes read into the buffer.
     * @throws std::system_error  I/O error reading from socket */
    virtual size_t recv(
           void* const  buf,
           const size_t len,
           const bool   peek = false);

    /**
     * Returns the size, in bytes, of the current record. Waits for a record if
     * necessary.
     * @retval 0  Stream is closed
     * @return Size, in bytes, of the current record
     */
    virtual size_t getSize();

    /**
     * Discards the current record.
     */
    virtual void discard();

    /**
     * Indicates if there's a current record.
     */
    virtual bool hasRecord();
};

/**
 * Output UDP socket. The remote endpoint of such a socket is bound to a
 * specific address.
 */
class OutUdpSock : public UdpSock
{
protected:
    class Impl;

private:
    Impl* getPimpl() const noexcept;

public:
    /**
     * Constructs an instance with unbound endpoints.
     */
    OutUdpSock() =default;

    /**
     * Constructs.
     * @param[in] remoteAddr  Internet address of the remote endpoint
     * @throws std::system_error  `socket()` failure
     */
    explicit OutUdpSock(const InetSockAddr& remoteAddr);

    /**
     * Destroys.
     */
    virtual ~OutUdpSock();

    /**
     * Returns the local socket address.
     * @return The local socket address
     * @throws std::bad_alloc  Necessary memory can't be allocated
     * @throws SystemError     Can't get local socket address
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    const InetSockAddr getLocalAddr() const;

    /**
     * Returns a string representation of this instance's socket.
     * @return String representation of this instance's socket
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    virtual std::string to_string() const;

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
    virtual const OutUdpSock& setHopLimit(
            const unsigned limit) const;

    /**
     * Scatter-sends a message.
     * @param[in] iovec     Vector comprising message to send
     * @param[in] iovcnt    Number of elements in `iovec`
     * @throws std::system_error  I/O error writing to socket
     */
    virtual void send(
            const struct iovec* const iovec,
            const int                 iovcnt);

    /**
     * Sends a message.
     * @param[in] buf       Buffer to send
     * @param[in] len       Size of buffer in bytes
     * @throws std::system_error  I/O error writing to socket
     */
    virtual void send(
            const void* const buf,
            const size_t      len);
};

/**
 * Multicast UDP socket. The remote endpoint of such a socket is bound to a
 * multicast group address.
 */
class McastUdpSock final : public InUdpSock
{
private:
    class Impl;

    Impl* getPimpl() const noexcept;

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
     * @param[in] sharePort  Whether or not the local port number should be
     *                       shared amongst multiple sockets
     */
    explicit McastUdpSock(
            const InetSockAddr& mcastAddr,
            const bool          sharePort = false);

    /**
     * Constructs a source-specific instance. The local and remote endpoints
     * will be bound to the given multicast group and only packets from the
     * source address will be accepted.
     * @param[in] mcastAddr   Address of multicast group
     * @param[in] sourceAddr  Address of source
     * @param[in] sharePort   Whether or not the local port number should be
     *                        shared amongst multiple sockets
     */
    McastUdpSock(
            const InetSockAddr& mcastAddr,
            const InetAddr&     sourceAddr,
            const bool          sharePort = false);

    /**
     * Destroys.
     */
    ~McastUdpSock();

    /**
     * Returns a string representation of this instance's socket.
     * @return String representation of this instance's socket
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    std::string to_string() const;
};

} // namespace

#endif
