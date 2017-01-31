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
#include "RecStream.h"

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
     * Default constructs.
     */
    UdpSock() =default;

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
     * Returns a string representation of this instance's socket.
     * @return String representation of this instance's socket
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    virtual std::string to_string() const =0;
};

/**
 * Input UDP socket. The local endpoint of such a socket is bound to a specific
 * address.
 */
class InUdpSock : virtual public UdpSock, public InRecStream
{
protected:
    class Impl;

private:
    inline Impl* getPimpl() const noexcept
    {
        return reinterpret_cast<Impl*>(pImpl.get());
    }

public:
    using InRecStream::recv;

    /**
     * Default constructs.
     */
    InUdpSock() =default;

    /**
     * Destroys.
     */
    ~InUdpSock() =default;

    /**
     * Constructs an instance that listens to a given address.
     * @param[in] srvrAddr  Internet address for the server
     */
    explicit InUdpSock(const InetSockAddr& srvrAddr);

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
    size_t recv(
            const struct iovec* iovec,
            const int           iovcnt,
            const bool          peek = false);

    /**
     * Returns the size, in bytes, of the current record. Waits for a record if
     * necessary.
     * @retval 0  Stream is closed
     * @return Size, in bytes, of the current record
     */
    size_t getSize();

    /**
     * Discards the current record.
     */
    void discard();

    /**
     * Indicates if there's a current record.
     */
    bool hasRecord();
};

/**
 * Output UDP socket. The remote endpoint of such a socket is bound to a
 * specific address.
 */
class OutUdpSock : virtual public UdpSock, public OutRecStream
{
protected:
    class Impl;

    OutUdpSock(Impl* const pImpl);

private:
    inline Impl* getPimpl() const noexcept
    {
        return reinterpret_cast<Impl*>(pImpl.get());
    }

public:
    using OutRecStream::send;

    /**
     * Constructs an instance with unbound endpoints.
     */
    OutUdpSock() =default;

    /**
     * Constructs.
     * @param[in] remoteAddr  Internet address of the remote endpoint
     */
    explicit OutUdpSock(const InetSockAddr& remoteAddr);

    /**
     * Destroys.
     */
    ~OutUdpSock() =default;

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
    const OutUdpSock& setHopLimit(
            const unsigned limit) const;

    /**
     * Sends a message.
     * @param[in] iovec     Vector comprising message to send
     * @param[in] iovcnt    Number of elements in `iovec`
     */
    void send(
            const struct iovec* const iovec,
            const int                 iovcnt);
};

/**
 * Multicast UDP socket. The remote endpoint of such a socket is bound to a
 * multicast group address.
 */
class McastUdpSock final : public InUdpSock, public OutUdpSock
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
    explicit McastUdpSock(const InetSockAddr& mcastAddr);

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
     * @return  This instance (to enable option-chaining)
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    const McastUdpSock& setMcastLoop(
            const bool enable) const;
};

} // namespace

#endif
