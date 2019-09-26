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

class InAddr
{
public:
    class                 Impl;

protected:
    std::shared_ptr<Impl> pImpl;

    InAddr(Impl* impl);

public:
    /**
     * Default constructs.
     */
    InAddr() noexcept;

    /**
     * Constructs from an IPv4 address in network byte order.
     *
     * @param[in] addr  IPv4 address in network byte order
     */
    InAddr(const in_addr_t addr) noexcept;

    /**
     * Constructs from an IPv4 address in network byte order.
     *
     * @param[in] addr  IPv4 address in network byte order
     */
    InAddr(const struct in_addr& addr) noexcept;

    /**
     * Constructs from an IPv6 address in network byte order.
     *
     * @param[in] addr  IPv6 address in network byte order
     */
    InAddr(const struct in6_addr& addr) noexcept;

    /**
     * Constructs from a string representation of an Internet address.
     *
     * @param[in] addr  String representation of Internet address
     */
    InAddr(const std::string& addr);

    inline operator bool() const noexcept
    {
        return (bool)pImpl;
    }

    int getFamily() const noexcept;

    /**
     * Returns the string representation of this instance.
     *
     * @return  String representation
     */
    std::string to_string() const;

    bool operator <(const InAddr& rhs) const noexcept;

    bool operator ==(const InAddr& rhs) const noexcept;

    size_t hash() const noexcept;

    /**
     * Returns a socket address corresponding to this instance and a port
     * number.
     *
     * @param[in] port  Port number
     * @return          Corresponding socket address
     */
    SockAddr getSockAddr(const in_port_t port) const;
};

} // namespace

#endif /* MAIN_NET_IO_IPADDR_H_ */
