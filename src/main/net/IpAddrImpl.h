/**
 * This file declares an abstract base class for an immutable IP address.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: IpAddr.h
 * @author: Steven R. Emmerson
 */

#ifndef IPADDR_H_
#define IPADDR_H_

#include "InetAddrImpl.h"

#include <netinet/in.h>
#include <sys/socket.h>

namespace hycast {

class IpAddrImpl : public InetAddrImpl {
public:
    virtual ~IpAddrImpl();
};

} // namespace

#endif /* IPADDR_H_ */
