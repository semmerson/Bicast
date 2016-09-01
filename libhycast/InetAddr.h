/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetAddr.h
 * @author: Steven R. Emmerson
 *
 * This file declares an Internet address.
 */

#ifndef INETADDR_H_
#define INETADDR_H_

#include <memory>
#include <netinet/in.h>
#include <string>

namespace hycast {

class InetAddrImpl; // Forward declaration

class InetAddr final {
    std::shared_ptr<InetAddrImpl> pImpl;
public:
    InetAddr();
    explicit InetAddr(const std::string ip_addr);
    explicit InetAddr(const in_addr_t  addr);
    explicit InetAddr(const struct in_addr& addr);
    explicit InetAddr(const struct in6_addr& addr);
    InetAddr(const InetAddr& that);
    int get_family() const;
    InetAddr& operator=(const InetAddr& rhs);
    size_t hash() const;
    int compare(const InetAddr& that) const;
    bool equals(const InetAddr& that) const {return compare(that) == 0;}
    std::string to_string() const;
};

} // namespace

#endif /* INETADDR_H_ */
