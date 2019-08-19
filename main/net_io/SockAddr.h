/**
 * Socket address. Can be IPv4, IPv6, or UNIX domain.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: SockAddr.h
 *  Created on: May 12, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_NET_IO_SOCKADDR_H_
#define MAIN_NET_IO_SOCKADDR_H_

#include "InAddr.h"

#include <memory>
#include <netinet/in.h>
#include <string>

namespace hycast {

class SockAddr
{
public:
    class Impl;

protected:
    std::shared_ptr<const Impl> pImpl;

    SockAddr(const Impl* const impl);

public:
    /**
     * Default constructs.
     */
    SockAddr() noexcept;

    /**
     * Constructs an IPv4 socket address.
     *
     * @param[in] addr  IPv4 address
     * @param[in] port  Port number in host byte-order
     */
    SockAddr(
            const in_addr_t addr,
            const in_port_t port);

    /**
     * Constructs an IPv4 socket address.
     *
     * @param[in] addr  IPv4 address
     * @param[in] port  Port number in host byte-order
     */
    SockAddr(
            const struct in_addr& addr,
            const in_port_t       port);

    /**
     * Constructs an IPv4 socket address.
     *
     * @param[in] sockaddr  IPv4 socket address
     */
    SockAddr(const struct sockaddr_in& sockaddr);

    /**
     * Constructs an IPv6 socket address.
     *
     * @param[in] addr  IPv6 address
     * @param[in] port  Port number in host byte-order
     */
    SockAddr(
            const struct in6_addr& addr,
            const in_port_t        port);

    /**
     * Constructs an IPv6 socket address.
     *
     * @param[in] sockaddr  IPv6 socket address
     */
    SockAddr(const struct sockaddr_in6& addr);

    /**
     * Constructs a socket address based on a hostname and port number.
     *
     * @param[in] name  Hostname
     * @param[in] port  Port number in host byte-order
     */
    SockAddr(
            const std::string& name,
            const in_port_t    port);

    /**
     * Constructs from a string specification.
     *
     * @param[in] spec  Socket specification. E.g.,
     *                    - host.name:38800
     *                    - 192.168.0.1:2400
     *                    - [fe80::20c:29ff:fe6b:3bda]:34084
     */
    SockAddr(const std::string& spec);

    inline operator bool() const noexcept
    {
        return (bool)pImpl;
    }

    /**
     * Clones this instance while changing the port number.
     *
     * @param[in] port      New port number
     */
    SockAddr clone(in_port_t port) const;

    /**
     * Indicates if this instance is considered less than another.
     *
     * @param[in] rhs      The other instance
     * @retval    `true`   This instance is less than `rhs`
     * @retval    `false`  This instance is not less than `rhs`
     */
    bool operator <(const SockAddr& rhs) const;

    /**
     * Returns the string representation of this instance.
     *
     * @return String representation of this instance
     */
    const std::string& to_string() const noexcept;

    /**
     * Returns the socket address structure corresponding to this instance.
     *
     * @param[out] sockaddr            Socket address structure
     * @paarm[out] socklen             Used size of `sockaddr`
     * @throws     std::system_error   `::getaddrinfo()` failure
     * @throws     std::runtime_error  Couldn't get IP address
     * @exceptionsafety                Strong guarantee
     * @threadsafety                   Safe
     */
    void getSockAddr(
            struct sockaddr& sockaddr,
            socklen_t&       socklen) const;

    /**
     * Returns the Internet address of this socket address.
     *
     * @return Internet address of this socket address
     */
    const InAddr& getInAddr() const noexcept;

    /**
     * Returns the port number given to the constructor in host byte-order.
     *
     * @return Constructor port number in host byte-order
     */
    in_port_t getPort() const;

    /**
     * Returns the address family of this socket address.
     *
     * @return  Address family. One of `AF_INET`, `AF_INET6`, or `AF_UNSPEC`.
     */
    int getFamily() const;
};

/******************************************************************************/

class SockAddrIn final : public SockAddr
{
public:

public:
    class Impl;

    SockAddrIn();

    /**
     * Constructs.
     *
     * @param[in] addr  IPv4 address in network byte-order
     * @param[in] port  Port number in host byte-order
     */
    SockAddrIn(
            const in_addr_t addr,
            const in_port_t port);
};

/******************************************************************************/

class SockAddrIn6 final : public SockAddr
{
public:
    class Impl;

    SockAddrIn6();

    /**
     * Constructs.
     *
     * @param[in] addr  IPv6 address in network byte-order
     * @param[in] port  Port number in host byte-order
     */
    SockAddrIn6(
            const struct in6_addr& addr,
            const in_port_t        port);
};

/******************************************************************************/

class SockAddrName final : public SockAddr
{
public:
    class Impl;

    SockAddrName();

    /**
     * Constructs from the name of a host.
     *
     * @param[in] name  Name of host. It's the caller's responsibility to
     *                  ensure that `name` isn't a string representation of an
     *                  IPv4 or IPv6 address.
     * @param[in] port  Port number in host byte-order
     */
    SockAddrName(
            const std::string& name,
            const in_port_t    port);
};

} // namespace

#endif /* MAIN_NET_IO_SOCKADDR_H_ */
