/**
 * This file declares a handle class for a multicast socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: McastSock.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_NET_MCASTSOCK_H_
#define MAIN_NET_MCASTSOCK_H_

#include "InetAddr.h"
#include "InetSockAddr.h"
#include "UdpSock.h"

namespace hycast {

class McastSock : public UdpSock {
public:
    /**
     * Constructs from a remote Internet socket address, a local Internet
     * interface address, and a time-to-live. The socket will be open for both
     * sending and receiving packets.
     * @param[in] sockAddr   Remote Internet socket address
     * @param[in] ifaceAddr  Local Internet interface address
     * @param[in] ttl        Time-to-live of outgoing packets (i.e., maximum
     *                       number of router hops):
     *                         -         0  Restricted to same host. Won't be
     *                                      output by any interface.
     *                         -         1  Restricted to the same subnet. Won't
     *                                      be forwarded by a router (default).
     *                         -    [2,31]  Restricted to the same site,
     *                                      organization, or department.
     *                         -   [32,63]  Restricted to the same region.
     *                         -  [64,127]  Restricted to the same continent.
     *                         - [128,254]  Unrestricted in scope. Global.
     * @throws std::invalid_argument  `ttl >= 255`
     * @throws std::system_error      Socket couldn't be created
     */
    McastSock(
            const InetSockAddr& sockAddr,
            const InetAddr&     ifaceAddr,
            const unsigned      ttl = 1);
        if (::setsockopt(sd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl))) {
            perror("Couldn't set time-to-live for multicast packets");
            exit(1);
        }
    struct in_addr iface;
    iface.s_addr = ifaceAddr;
    if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &iface, sizeof(iface))) {
        perror("Couldn't set sending interface");
        exit(1);
    }
};

} // namespace

#endif /* MAIN_NET_MCASTSOCK_H_ */
