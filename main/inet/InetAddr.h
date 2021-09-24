/**
 * Internet address. Could be IPv4 or IPv6.
 *
 *        File: InAddr.h
 *  Created on: May 6, 2019
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

#ifndef MAIN_NET_IO_INADDR_H_
#define MAIN_NET_IO_INADDR_H_

#include <memory>
#include <netinet/in.h>

namespace hycast {

/******************************************************************************
 * Transport API
 ******************************************************************************/

class SockAddr;
class TcpSock;
class UdpSock;
class XprtAble;

/// Transport
class Xprt
{
public:
    class Impl; // Implementation

protected:
    std::shared_ptr<Impl> pImpl;

public:
    Xprt() =default;

    /**
     * Constructs.
     *
     * @param[in] sock  TCP socket
     */
    explicit Xprt(TcpSock& sock);

    /**
     * Constructs.
     *
     * @param[in] sock  UDP socket
     */
    explicit Xprt(UdpSock& sock);

    operator bool() {
        return static_cast<bool>(pImpl);
    }

    SockAddr getRmtAddr() const;

    std::string to_string() const;

    /**
     * Transports a boolean to the remote host.
     *
     * @param[in] pduId    PDU identifier
     * @param[in] value    Boolean to be transported
     * @retval    `true`   Success
     * @retval    `false`  Connection lost
     */
    bool send(unsigned pduId, const bool value) const;

    /**
     * Transports an object to the remote host.
     *
     * @param[in] pduId    PDU identifier
     * @param[in] obj      Object to be transported
     * @retval    `true`   Success
     * @retval    `false`  Connection lost
     */
    bool send(unsigned pduId, const XprtAble& obj);

    /**
     * Receives the next, incoming PDU.
     *
     * @param[out] pduid    Identifier of the next PDU
     * @retval     `true`   Success
     * @retval     `false`  Connection lost
     */
    bool recv(unsigned& pduId);

    bool write(const void*        value, size_t nbytes);
    bool write(const bool         value);
    bool write(const uint8_t      value);
    bool write(const uint16_t     value);
    bool write(const uint32_t     value);
    bool write(const uint64_t     value);
    bool write(const std::string& value);

    bool read(void*        value, size_t nbytes);
    bool read(bool&        value);
    bool read(uint8_t&     value);
    bool read(uint16_t&    value);
    bool read(uint32_t&    value);
    bool read(uint64_t&    value);
    bool read(std::string& value);

    void shutdown();
};

/// Interface for a transportable object
class XprtAble
{
public:
    virtual ~XprtAble() {};

    virtual bool write(Xprt& xprt) const =0;

    virtual bool read(Xprt& xprt) =0;
};

/******************************************************************************
 * Internet Addresses
 ******************************************************************************/

class SockAddr;

class InetAddr : public XprtAble
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

    operator bool() const noexcept;

    int getFamily() const noexcept;

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
     * Returns a socket descriptor appropriate to this instance's address
     * family.
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
            const int protocol) const;

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

    /**
     * Indicates if this instance is a valid, source-specific multicast address
     * that is not reserved for allocation by IANA. I.e., this instance is in
     * the range from 232.0.1.0 through 232.255.255.255 (for IPv4) or
     * FF3X::0000 through FF3X::4000:0000 or FF3X::8000:0000 through
     * FF3X::FFFF:FFFF (for IPv6).
     *
     * @retval `true`   Address is valid and in appropriate range
     * @retval `false`  Address is invalid or not in appropriate range
     */
    bool isSsm() const;

    /**
     * Writes to a transport.
     *
     * @param[in] xprt     Transport
     * @retval    `true`   Success
     * @retval    `false`  Connection lost
     */
    bool write(Xprt& xprt) const;

    /**
     * Reads from a transport.
     *
     * @param[in] xprt     Transport
     * @retval    `true`   Success
     * @retval    `false`  Connection lost
     */
    bool read(Xprt& xprt);
};

} // namespace

#endif /* MAIN_NET_IO_IPADDR_H_ */
