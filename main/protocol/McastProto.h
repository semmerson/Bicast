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

#include <main/inet/SockAddr.h>
#include <main/inet/Socket.h>
#include "hycast.h"
#include <memory>

namespace hycast {

/**
 * Source-specific multicast information
 */
struct SrcMcastAddrs
{
    SockAddr grpAddr;
    InetAddr srcAddr;
};

/******************************************************************************/

/**
 * Class for holding multicast protocol parameters.
 */
class McastProto
{
public:
    /// Maximum size of a data-segment in bytes
    static const int MAX_SEGSIZE = UdpSock::MAX_PAYLOAD - 12;
};

/******************************************************************************/

/**
 * Multicasts data-products.
 */
class McastSndr
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

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
     * Constructs.
     *
     * @param[in] sock         UDP socket
     * @throw     SystemError  I/O failure
     * @cancellationpoint      Yes
     */
    McastSndr(UdpSock&& sock);

    /**
     * Sets the interface to use for multicasting.
     *
     * @param[in] iface        Interface to use for multicasting
     * @return                 This instance
     * @throw     SystemError  I/O failure
     * @cancellationpoint      Yes
     */
    const McastSndr& setMcastIface(const InetAddr& iface) const;

    /**
     * Multicasts product-information.
     *
     * @param[in] info          Product information
     * @throws    SystemError   I/O failure
     * @cancellationpoint       Yes
     */
    void multicast(const ProdInfo& info);

    /**
     * Multicasts a data-segment.
     *
     * @param[in] seg           Data segment
     * @throws    SystemError   I/O failure
     * @cancellationpoint       Yes
     */
    void multicast(const MemSeg& seg);
};

/******************************************************************************/

/**
 * Interface for a subscriber of a multicast products.
 */
class McastSub
{
public:
    virtual ~McastSub()
    {}

    virtual bool hereIsMcast(const ProdInfo& prodInfo) =0;

    virtual bool hereIsMcast(UdpSeg& seg) =0;
};

/******************************************************************************/

/**
 * Receives multicast data-products.
 */
class McastRcvr
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs.
     *
     * @param[in] srcMcastInfo  Source-specific multicast information
     * @param[in] mcastSub      Subscriber of multicast products
     * @cancellationpoint       Yes
     */
    McastRcvr(
            const SrcMcastAddrs& srcMcastInfo,
            McastSub&            mcastSub);

    /**
     * Executes the multicast receiver. Calls this instance's observer. Returns
     * on EOF.
     *
     * @throws SystemError   I/O error
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
