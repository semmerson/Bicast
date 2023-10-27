/**
 * @file: SockAddr.h
 * Socket address module.
 * IPv4, IPv6, and hostname-based socket addresses.
 *
 *  Created on: May 12, 2019
 *      Author: Steven R. Emmerson
 *
 *  Copyright 2023 University Corporation for Atmospheric Research
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
#include "XprtAble.h"

#include <functional>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <string>

namespace bicast {

/// A socket address. Socket address comprise an Internet address and a port number.
class SockAddr : public XprtAble
{
    class                 Impl;  ///< An implementation
    std::shared_ptr<Impl> pImpl; ///< Smart pointer to an implementation

public:
    /**
     * Default constructs. The resulting instance will test false.
     * @see operator bool()
     */
    SockAddr() noexcept;

    /**
     * Constructs from an Internet address and an optional port number.
     *
     * @param[in] inetAddr  Internet address
     * @param[in] port      Optional port number in host byte-order
     * @param[in] port      Optional port number in host byte-order. `0` obtains a system-chosen
     *                      port number.
     */
    explicit SockAddr(const InetAddr& inetAddr,
             in_port_t                port = 0);

    /**
     * Constructs from an IPv4 address and an optional port number.
     * @param[in] addr  IPv4 address
     * @param[in] port  Optional port number in host byte-order. `0` obtains a system-chosen port
     *                  number.
     */
    explicit SockAddr(
            const in_addr_t addr,
            in_port_t       port = 0);

    /**
     * Constructs from an IPv4 address and an optional port number.
     * @param[in] addr  IPv4 address
     * @param[in] port  Optional port number in host byte-order.  `0` obtains a system-chosen port
     *                  number.
     */
    explicit SockAddr(
            const struct in_addr& addr,
            in_port_t             port = 0);

    /**
     * Constructs from an IPv4 socket address structure.
     *
     * @param[in] sockaddr  IPv4 socket address
     */
    explicit SockAddr(const struct sockaddr_in& sockaddr);

    /**
     * Constructs from an IPv6 socket address structure and an optional port number.
     *
     * @param[in] addr  IPv6 address
     * @param[in] port  Optional port number in host byte-order. `0` obtains a system-chosen port
     *                  number.
     */
    explicit SockAddr(
            const struct in6_addr& addr,
            in_port_t              port = 0);

    /**
     * Constructs from an IPv6 socket address.
     *
     * @param[in] addr  IPv6 socket address
     */
    explicit SockAddr(const struct sockaddr_in6& addr);

    /**
     * Constructs from a generic socket address.
     *
     * @param[in] sockaddr               Generic socket address
     * @throws    std::invalid_argument  Address family isn't supported
     */
    explicit SockAddr(const struct sockaddr& sockaddr);

    /**
     * Constructs from a generic socket address.
     *
     * @param[in] storage                Generic socket address
     * @throws    std::invalid_argument  Address family isn't supported
     */
    explicit SockAddr(const struct sockaddr_storage& storage);

    /**
     * Constructs from a socket string-specification.
     *
     * @param[in] spec  Socket specification. E.g.,
     *                    - host.name:38800
     *                    - 192.168.0.1:2400
     *                    - [fe80::20c:29ff:fe6b:3bda]:34084
     *                  The colon and port number specification is optional. If it's not specified,
     *                  then the optional argument is used.
     * @param[in] port  Optional port number in host byte-order. `0` obtains a system-chosen port
     *                  number.
     */
    explicit SockAddr(
            const std::string& spec,
            in_port_t          port = 0);

    /**
     * Destroys.
     */
    ~SockAddr() {
        pImpl.reset();
    }

    /**
     * Copy assigns from another instance.
     * @param[in] rhs  The other instance
     * @return         This instance
     */
    SockAddr& operator=(const SockAddr& rhs) {
        pImpl = rhs.pImpl;
        return *this;
    }

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
    InetAddr getInetAddr() const noexcept;

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

std::ostream& operator<<(std::ostream& ostream, const SockAddr& addr);

} // namespace

namespace std {
    using namespace bicast;

    std::string to_string(const SockAddr& sockAddr);

    /// Less-than class function for a socket address
    template<>
    struct less<SockAddr> {
        /**
         * Indicates if one socket address is less than another.
         * @param[in] lhs      The first socket address
         * @param[in] rhs      The second socket address
         * @retval    true     The first address is less than the second
         * @retval    false    The first address is not less than the second
         */
        bool operator()(
                const SockAddr& lhs,
                const SockAddr& rhs) {
            return lhs < rhs;
        }
    };

    /// Hash code class function for a socket address
    template<>
    struct hash<SockAddr> {
        /**
         * Returns the hash code of a socket address.
         * @param[in] sockAddr  The socket address
         * @return The hash code of the socket address
         */
        size_t operator()(const SockAddr& sockAddr) const {
            return sockAddr.hash();
        }
    };
}

#endif /* MAIN_INET_SOCKADDR_H_ */
