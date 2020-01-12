/**
 * Sends data-product via Hycast.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Sender.h
 *  Created on: Jan 3, 2020
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_SENDER_SENDER_H_
#define MAIN_SENDER_SENDER_H_

#include "Socket.h"
#include "PortPool.h"
#include "Repository.h"
#include "ServerPool.h"

#include <memory>

namespace hycast {

class Sender
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    Sender(Impl* const impl);

public:
    /**
     * Constructs.
     *
     * @param[in] srvrAddr    Address to be used by this instance's server
     * @param[in] listenSize  Size of the server's `::listen()` queue
     * @param[in] portPool    Poll of available port numbers for subsequent
     *                        connections with peers
     * @param[in] maxPeers    Maximum number of peers
     * @param[in] grpAddr     Address of multicast group for multicasting
     *                        products
     * @param[in] repo        Repository of transitory data-products to be sent
     */
    Sender(
        const SockAddr& srvrAddr,
        int             listenSize,
        PortPool&       portPool,
        int             maxPeers,
        const SockAddr& grpAddr,
        SrcRepo&        repo);

    void send(const ProdInfo& prodInfo) const;

    void send(const MemSeg& memSeg) const;
};

} // namespace

#endif /* MAIN_SENDER_SENDER_H_ */
