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

#include "InetAddr.h"

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
     * Constructs from an IPv4 socket address.
     *
     * @param[in] addr  IPv4 address
     * @param[in] port  Port number in host byte-order. `0` obtains a system-
     *                  chosen port number.
     */
    SockAddr(
            const in_addr_t addr,
            const in_port_t port);

    /**
     * Constructs from an IPv4 socket address.
     *
     * @param[in] addr  IPv4 address
     * @param[in] port  Port number in host byte-order. `0` obtains a system-
     *                  chosen port number.
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
     * Constructs from an IPv6 socket address. `0` obtains a system-chosen port
     * number.
     *
     * @param[in] addr  IPv6 address
     * @param[in] port  Port number in host byte-order
     */
    SockAddr(
            const struct in6_addr& addr,
            const in_port_t        port);

    /**
     * Constructs from an IPv6 socket address.
     *
     * @param[in] sockaddr  IPv6 socket address
     */
    SockAddr(const struct sockaddr_in6& addr);

    /**
     * Constructs from a generic socket address.
     *
     * @param[in] sockaddr               Generic socket address
     * @throws    std::invalid_argument  Address family isn't supported
     */
    SockAddr(const struct sockaddr& sockaddr);

    /**
     * Constructs from a hostname and port number.
     *
     * @param[in] name  Hostname
     * @param[in] port  Port number in host byte-order. `0` obtains a system-
     *                  chosen port number.
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
     * Returns a socket appropriate for this instance's address family.
     *
     * @param[in] type               Type of socket. One of `SOCK_STREAM`,
     *                               `SOCK_DGRAM`, or `SOCK_SEQPACKET`.
     * @param[in] protocol           Protocol. E.g., `IPPROTO_TCP` or `0` to
     *                               obtain the default protocol.
     * @return                       Appropriate socket
     * @throws    std::system_error  `::socket()` failure
     */
    int socket(
            const int type,
            const int protocol = 0) const;

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
     * Returns the hash value of this instance.
     *
     * @return The hash value of this instance
     */
    size_t hash() const noexcept;

    /**
     * Returns the string representation of this instance.
     *
     * @return String representation of this instance
     */
    const std::string& to_string() const noexcept;

    /**
     * Binds a socket to a local socket address.
     *
     * @param[in] sd                 Socket descriptor
     * @throws    std::system_error  `::bind()` failure
     * @threadsafety                 Safe
     */
    void bind(const int sd) const;

    /**
     * Connects a socket to a remote socket address.
     *
     * @param[in] sd                 Socket descriptor
     * @throws    std::system_error  `::connect()` failure
     * @threadsafety                 Safe
     */
    void connect(const int sd) const;

    /**
     * Returns the Internet address of this socket address.
     *
     * @return Internet address of this socket address
     */
    const InetAddr& getInAddr() const noexcept;

    /**
     * Returns the port number given to the constructor in host byte-order.
     *
     * @return            Constructor port number in host byte-order
     * @cancellationpoint No
     */
    in_port_t getPort() const noexcept;
};

} // namespace

namespace std {
    template<>
    struct less<hycast::SockAddr> {
        inline bool operator()(
                const hycast::SockAddr& lhs,
                const hycast::SockAddr& rhs) {
            return lhs < rhs;
        }
    };

    template<>
    struct hash<hycast::SockAddr> {
        inline bool operator()(const hycast::SockAddr& sockAddr) const {
            return sockAddr.hash();
        }
    };
}

#endif /* MAIN_NET_IO_SOCKADDR_H_ */
