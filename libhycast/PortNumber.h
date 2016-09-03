/**
 * This file declares an immutable Internet port number.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PortNumber.h
 * @author: Steven R. Emmerson
 */

#ifndef PORTNUMBER_H_
#define PORTNUMBER_H_

#include <arpa/inet.h>
#include <cstdint>
#include <netinet/in.h>
#include <string>

namespace hycast {

class PortNumber final {
    in_port_t port;
public:
    /**
     * Default constructor. The port number will be 0.
     * @exceptionsafety Nothrow
     */
    PortNumber() : port{0} {}
    /**
     * Constructs from a port number in host byte-order.
     * @param[in] host_order  Port number in host byte-order.
     * @exceptionsafety Nothrow
     */
    explicit PortNumber(const in_port_t host_order) noexcept : port{host_order} {};
    /**
     * Returns the port number in network byte-order.
     * @return The port number in network byte-order.
     * @exceptionsafety Nothrow
     */
    in_port_t get_network() const noexcept {
        return htons(port);
    }
    /**
     * Returns the port number in host byte-order.
     * @return The port number in host byte-order.
     * @exceptionsafety Nothrow
     */
    in_port_t get_host() const noexcept {
        return port;
    }
    /**
     * Returns the string representation of the port number.
     * @return The string representation of the port number.
     * @exceptionsafety Strong
     */
    std::string to_string() const {
        return std::to_string(port);
    }
};

} // namespace

#endif /* PORTNUMBER_H_ */
