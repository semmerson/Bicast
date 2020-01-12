/**
 * Internet address. Could be IPv4 or IPv6.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: InAddr.h
 *  Created on: May 6, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_NET_IO_INADDR_H_
#define MAIN_NET_IO_INADDR_H_

#include <memory>
#include <netinet/in.h>

namespace hycast {

class SockAddr;

class InetAddr
{
public:
    class                 Impl;

protected:
    std::shared_ptr<Impl> pImpl;

    InetAddr(Impl* impl);

public:
    /**
     * Default constructs.
     */
    InetAddr() noexcept;

    /**
     * Constructs from an IPv4 address in network byte order.
     *
     * @param[in] addr  IPv4 address in network byte order
     */
    InetAddr(const in_addr_t addr) noexcept;

    /**
     * Constructs from an IPv4 address in network byte order.
     *
     * @param[in] addr  IPv4 address in network byte order
     */
    InetAddr(const struct in_addr& addr) noexcept;

    /**
     * Constructs from an IPv6 address in network byte order.
     *
     * @param[in] addr  IPv6 address in network byte order
     */
    InetAddr(const struct in6_addr& addr) noexcept;

    /**
     * Constructs from a string representation of an Internet address.
     *
     * @param[in] addr  String representation of Internet address
     */
    InetAddr(const std::string& addr);

    inline operator bool() const noexcept
    {
        return (bool)pImpl;
    }

    /**
     * Returns the string representation of this instance.
     *
     * @return  String representation
     */
    std::string to_string() const;

    bool operator <(const InetAddr& rhs) const noexcept;

    bool operator ==(const InetAddr& rhs) const noexcept;

    size_t hash() const noexcept;

    /**
     * Returns a socket address corresponding to this instance and a port
     * number.
     *
     * @param[in] port  Port number
     * @return          Corresponding socket address
     */
    SockAddr getSockAddr(const in_port_t port) const;

    /**
     * Joins the source-specific multicast group identified by this instance
     * and the address of the sending host.
     *
     * @param[in] sd       Socket identifier
     * @param[in] srcAddr  Address of the sending host
     * @threadsafety       Safe
     * @exceptionsafety    Strong guarantee
     * @cancellationpoint  Maybe (`::getaddrinfo()` may be one and will be
     *                     called if either address is based on a name)
     */
    void join(
            const int       sd,
            const InetAddr& srcAddr) const;

    /**
     * Sets a socket address structure and returns a pointer to it.
     *
     * @param[in] storage  Socket address structure
     * @param[in] port     Port number in host byte-order
     * @return             Pointer to given socket address structure
     * @threadsafety       Safe
     */
    struct sockaddr* get_sockaddr(
            struct sockaddr_storage& storage,
            const in_port_t          port) const;

    /**
     * Set a UDP socket to use the interface associated with this instance.
     *
     * @param[in] sd          UDP socket descriptor
     * @return                This instance
     * @throws    LogicError  This instance is based on a hostname and not an
     *                        IP address
     * @threadsafety          Safe
     * @exceptionsafety       Strong guarantee
     * @cancellationpoint     Unknown due to non-standard function usage
     */
    const InetAddr& setMcastIface(int sd) const;
};

} // namespace

#endif /* MAIN_NET_IO_IPADDR_H_ */
