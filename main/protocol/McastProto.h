/**
 * Dispatcher of incoming messages to appropriate methods.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Dispatcher.h
 *  Created on: Nov 4, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PROTOCOL_INPROTO_H_
#define MAIN_PROTOCOL_INPROTO_H_

#include "hycast.h"
#include "Socket.h"

#include <memory>

namespace hycast {

/**
 * Multicasts data-products.
 */
class McastSndr
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    McastSndr(Impl* impl);

public:
    /**
     * Constructs.
     *
     * @param[in] sock         UDP socket
     * @throw     SystemError  I/O failure
     * @cancellationpoint      Yes
     */
    McastSndr(UdpSock& sock);

    /**
     * Multicasts product-information.
     *
     * @param[in] info          Product information
     * @throws    SystemError   I/O failure
     * @cancellationpoint       Yes
     */
    void send(ProdInfo& info);

    /**
     * Multicasts a data-segment.
     *
     * @param[in] seg           Data segment
     * @throws    SystemError   I/O failure
     * @cancellationpoint       Yes
     */
    void send(MemSeg& seg);
};

/**
 * Interface for an observer of an McastRcvr.
 */
class McastRcvrObs
{
public:
    virtual ~McastRcvrObs()
    {}

    virtual void hereIs(const ProdInfo& prodInfo) =0;

    virtual void hereIs(UdpSeg& seg) =0;
};

/**
 * Receives multicast data-products.
 */
class McastRcvr
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    McastRcvr(Impl* impl);

public:
    /**
     * Constructs.
     *
     * @param[in] sock     UDP socket
     * @param[in] msgRcvr  Services multicast messages
     * @cancellationpoint  Yes
     */
    McastRcvr(
            UdpSock&      sock,
            McastRcvrObs& msgRcvr);

    /**
     * Executes the multicast receiver. Calls the multicast message server.
     * Returns on EOF.
     *
     * @throws RuntimeError  I/O error
     * @cancellationpoint    Yes
     */
    void operator()();

    /**
     * Halts the multicast receiver by shutting down the UDP socket for reading.
     * Causes `operator()()` to return.
     *
     * @throws SystemError   `::shutdown()` failure
     * @cancellationpoint    No
     */
    void halt();
};

} // namespace

#endif /* MAIN_PROTOCOL_INPROTO_H_ */
