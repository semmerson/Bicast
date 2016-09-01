/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PortNumber.h
 * @author: Steven R. Emmerson
 *
 * This file defines an Internet port number.
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
    PortNumber() : port{0} {}
    explicit PortNumber(const in_port_t host_order) : port{host_order} {};
    in_port_t get_network() const {
        return htons(port);
    }
    in_port_t get_host() const {
        return port;
    }
    std::string to_string() const {
        return std::to_string(port);
    }
};

} // namespace

#endif /* PORTNUMBER_H_ */
