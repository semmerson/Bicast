/**
 * Socket address. Can be IPv4, IPv6, or UNIX domain.
 *
 *        File: SockAddr.h
 *  Created on: May 12, 2019
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

#ifndef MAIN_INET_SOCKADDR_H_
#define MAIN_INET_SOCKADDR_H_

#include "InetAddr.h"

#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <string>

namespace hycast {

/// A socket address. Socket address comprise an Internet address and a port number.
class SockAddr : public XprtAble
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs.
     */
    SockAddr() noexcept;

    /**
     * Constructs from an Internet address.
     *
     * @param[in] inetAddr  Internet address
     * @param[in] port      Port number in host byte-order
     */
    SockAddr(const InetAddr& inetAddr,
             const in_port_t port);

    /**
     * Constructs from an IPv4 socket address.
     *
     * @param[in] addr  IPv4 address
     * @param[in] port  Port number in host byte-order. `0` obtains a system-chosen port number.
     */
    SockAddr(const in_addr_t addr,
             const in_port_t port);

    /**
     * Constructs from an IPv4 socket address.
     *
     * @param[in] addr  IPv4 address
     * @param[in] port  Port number in host byte-order. `0` obtains a system-chosen port number.
     */
    SockAddr(const struct in_addr& addr,
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
    SockAddr(const struct in6_addr& addr,
             const in_port_t        port);

    /**
     * Constructs from an IPv6 socket address.
     *
     * @param[in] addr  IPv6 socket address
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
     * Constructs from a generic socket address.
     *
     * @param[in] storage                Generic socket address
     * @throws    std::invalid_argument  Address family isn't supported
     */
    SockAddr(const struct sockaddr_storage& storage);

    /**
     * Constructs from a hostname and port number.
     *
     * @param[in] name  Hostname
     * @param[in] port  Port number in host byte-order. `0` obtains a system-
     *                  chosen port number, eventually.
     */
    SockAddr(const std::string& name,
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

    /**
     * Clones this instance and changes the port number.
     *
     * @param[in] port      New port number in host byte-order
     */
    SockAddr clone(in_port_t port) const;

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     * @retval true     This instance is valid
     * @retval false    This instance is not valid
     */
    operator bool() const noexcept;

    /**
     * Returns the Internet address of this socket address.
     *
     * @return Internet address of this socket address
     */
    const InetAddr getInetAddr() const noexcept;

    /**
     * Returns the port number given to the constructor in host byte-order.
     *
     * @return            Constructor port number in host byte-order
     * @cancellationpoint No
     */
    in_port_t getPort() const noexcept;

    /**
     * Returns the string representation of this instance.
     *
     * @return String representation of this instance
     */
    std::string to_string(const bool withName = false) const noexcept;

    /**
     * Returns the hash value of this instance.
     *
     * @return The hash value of this instance
     */
    size_t hash() const noexcept;

    /**
     * Sets a socket address storage structure.
     *
     * @param[out] storage  The structure to be set
     * @return              Pointer to the structure
     * @cancellationpoint   Maybe (`::getaddrinfo()` may be one and will be
     *                      called if the address is based on a name)
     */
    struct sockaddr* get_sockaddr(struct sockaddr_storage& storage) const;

    /**
     * Indicates if this instance is considered less than another.
     *
     * @param[in] rhs      The other instance
     * @retval    true     This instance is less than `rhs`
     * @retval    false    This instance is not less than `rhs`
     */
    bool operator <(const SockAddr& rhs) const noexcept;

    /**
     * Indicates if this instance is considered equal to another.
     *
     * @param[in] rhs      The other instance
     * @retval    true     This instance is equal to `rhs`
     * @retval    false    This instance is not equal to `rhs`
     */
    bool operator ==(const SockAddr& rhs) const noexcept;

    /**
     * Indicates if this instance is not considered equal to another.
     *
     * @param[in] rhs      The other instance
     * @retval    true     This instance is not equal to `rhs`
     * @retval    false    This instance is equal to `rhs`
     */
    bool operator !=(const SockAddr& rhs) const noexcept {
        return !(*this == rhs);
    }

    /**
     * Binds a socket to a local socket address.
     *
     * @param[in] sd                 Socket descriptor
     * @throws    std::system_error  Bind failure
     * @threadsafety                 Safe
     */
    void bind(const int sd) const;

    /**
     * Writes to a transport.
     *
     * @param[in] xprt     Transport
     * @retval    true     Success
     * @retval    false    Connection lost
     */
    bool write(Xprt xprt) const;

    /**
     * Reads from a transport.
     *
     * @param[in] xprt     Transport
     * @retval    true     Success
     * @retval    false    Connection lost
     */
    bool read(Xprt xprt);
};

std::ostream& operator<<(std::ostream& ostream, const SockAddr& addr);

} // namespace

namespace std {
    std::string to_string(const hycast::SockAddr& sockAddr);

    /// Less-than class function for a socket address
    template<>
    struct less<hycast::SockAddr> {
        /**
         * Indicates if one socket address is less than another.
         * @param[in] lhs      The first socket address
         * @param[in] rhs      The second socket address
         * @retval    true     The first address is less than the second
         * @retval    false    The first address is not less than the second
         */
        inline bool operator()(
                const hycast::SockAddr& lhs,
                const hycast::SockAddr& rhs) {
            return lhs < rhs;
        }
    };

    /// Hash code class function for a socket address
    template<>
    struct hash<hycast::SockAddr> {
        /**
         * Returns the hash code of a socket address.
         * @param[in] sockAddr  The socket address
         * @return The hash code of the socket address
         */
        inline bool operator()(const hycast::SockAddr& sockAddr) const {
            return sockAddr.hash();
        }
    };
}

#endif /* MAIN_INET_SOCKADDR_H_ */
