/**
 * Internet address. Could be IPv4 or IPv6.
 *
 *        File: InAddr.h
 *  Created on: May 6, 2019
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2023 University Corporation for Atmospheric Research
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

#ifndef MAIN_NET_IO_INADDR_H_
#define MAIN_NET_IO_INADDR_H_

#include "XprtAble.h"

#include <iostream>
#include <memory>
#include <netinet/in.h>

namespace bicast {

/******************************************************************************
 * Internet Addresses
 ******************************************************************************/

/// An Internet address
class InetAddr : public XprtAble
{
public:
    class                 Impl;

protected:
    /// Smart pointer to the implementation
    std::shared_ptr<Impl> pImpl;

    /**
     * Constructs.
     * @param[in] impl  Pointer to an implementation
     */
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
    explicit InetAddr(const in_addr_t addr) noexcept;

    /**
     * Constructs from an IPv4 address in network byte order.
     *
     * @param[in] addr  IPv4 address in network byte order
     */
    explicit InetAddr(const struct in_addr& addr) noexcept;

    /**
     * Constructs from an IPv6 address in network byte order.
     *
     * @param[in] addr  IPv6 address in network byte order
     */
    explicit InetAddr(const struct in6_addr& addr) noexcept;

    /**
     * Constructs from a string representation of an Internet address.
     *
     * @param[in] addr  String representation of Internet address
     */
    explicit InetAddr(const std::string& addr);

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     * @retval true     This instance is valid
     * @retval false    This instance is not valid
     */
    operator bool() const noexcept;

    /**
     * Returns the address family: `AF_INET` or `AF_INET6`.
     * @return The address family
     */
    int getFamily() const;

    /**
     * Indicates if this address is link-local (i.e., behind a NAT device on a LAN) and will not
     * be routed.
     * @retval true   This address is link-local
     * @retval false  This address is not link-local
     */
    bool isLinkLocal() const;

    /**
     * Returns the index of the interface associated with this IP address.
     *
     * @return Interface index associated with this IP address
     */
    unsigned getIfaceIndex() const;

    /**
     * Indicates if this instance specifies any interface (e.g., `INADDR_ANY`).
     *
     * @retval true     Address does specify any interface
     * @retval false    Address does not specify any interface
     */
    bool isAny() const;

    /**
     * Indicates if this instance is a multicast address.
     *
     * @retval true     Address is a multicast address
     * @retval false    Address is not a multicast address
     */
    bool isMulticast() const;

    /**
     * Indicates if this instance is a valid, source-specific multicast address
     * that is not reserved for allocation by IANA. I.e., this instance is in
     * the range from 232.0.1.0 through 232.255.255.255 (for IPv4) or
     * FF3X::0000 through FF3X::4000:0000 or FF3X::8000:0000 through
     * FF3X::FFFF:FFFF (for IPv6).
     *
     * @retval true     Address is valid and in appropriate range
     * @retval false    Address is invalid or not in appropriate range
     */
    bool isSsm() const;

    /**
     * Returns the wildcard address for an address family.
     *
     * @param[in] family       Internet address family: `AF_INET`, `AF_INET6`
     * @return                 Corresponding wildcard address
     * @throw InvalidArgument  Invalid address family
     */
    static InetAddr getWildcard(const int family);

    /**
     * Returns the wildcard address for the address family of this instance.
     *
     * @return Wildcard address for this instance's address family
     */
    InetAddr getWildcard() const;

    /**
     * Returns the string representation of this instance.
     *
     * @return  String representation
     */
    std::string to_string() const;

    /**
     * Indicates if this instance is less than another.
     * @param[in] rhs     The other instance
     * @retval    true    This instance is less than the other
     * @retval    false   This instance is not less than the other
     */
    bool operator<(const InetAddr& rhs) const noexcept;

    /**
     * Indicates if this instance is equal to another.
     * @param[in] rhs     The other instance
     * @retval    true    This instance is equal to the other
     * @retval    false   This instance is not equal to the other
     */
    bool operator==(const InetAddr& rhs) const noexcept;

    /**
     * Returns the hash code of this instance.
     * @return The hash code of this instance
     */
    size_t hash() const noexcept;

    /**
     * Returns a socket descriptor appropriate to this instance's address
     * family.
     *
     * @param[in] type               Type of socket. One of `SOCK_STREAM`,
     *                               `SOCK_DGRAM`, or `SOCK_SEQPACKET`.
     * @param[in] protocol           Protocol. E.g., `IPPROTO_TCP` or `0` to
     *                               obtain the default protocol.
     * @return                       Appropriate socket
     * @throws    std::system_error  `socket()` failur
     */
    int socket(
            const int type,
            const int protocol = 0) const;

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
     * Makes the given socket use the interface associated with this instance.
     *
     * @param[in] sd               UDP socket descriptor
     * @return                     This instance
     * @throws    LogicError       This instance is based on a hostname and not an IP address
     * @throw     InvalidArgument  Socket's address family != this instances
     * @threadsafety               Safe
     * @exceptionsafety            Strong guarantee
     * @cancellationpoint          Unknown due to non-standard function usage
     */
    const InetAddr& makeIface(int sd) const;

    /**
     * Writes to a transport.
     *
     * @param[in] xprt     Transport
     * @retval    true     Success
     * @retval    false    Connection lost
     */
    bool write(Xprt& xprt) const;

    /**
     * Reads from a transport.
     *
     * @param[in] xprt     Transport
     * @retval    true     Success
     * @retval    false    Connection lost
     */
    bool read(Xprt& xprt);
};

std::ostream& operator<<(std::ostream& ostream, const InetAddr& addr);

} // namespace

#endif /* MAIN_NET_IO_IPADDR_H_ */
