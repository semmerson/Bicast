/**
 * This file declares a receiver of multicast datagrams. The multicast can be
 * either any-source or source-specific.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: McastReceiver.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_MCAST_MCASTRECEIVER_H_
#define MAIN_MCAST_MCASTRECEIVER_H_

#include <memory>

#include "McastContentRcvr.h"

namespace hycast {

/**
 * Information on a source-specific multicast.
 */
struct SrcMcastInfo final
{
	InetSockAddr mcastAddr; /// Socket address of multicast group
	InetAddr     srcAddr;   /// Internet address of multicast source
};

class McastReceiver
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs a source-independent multicast receiver.
     * @param[in] mcastAddr  Multicast group socket address
     * @param[in] msgRcvr    Receiver of multicast messages. Must exist for the
     *                       duration of the constructed instance.
     * @param[in] version    Protocol version
     */
    McastReceiver(
            const InetSockAddr& mcastAddr,
            McastContentRcvr&       msgRcvr,
            const unsigned      version);

    /**
     * Constructs an any-source multicast receiver.
     * @param[in] mcastAddr  Internet socket address of the multicast group
     * @param[in] srcAddr    Internet address of the source of multicast
     *                       datagrams
     * @param[in] msgRcvr    Receiver of multicast messages. Must exist for the
     *                       duration of the constructed instance.
     * @param[in] version    Protocol version
     */
    McastReceiver(
            const InetSockAddr& mcastAddr,
            const InetAddr&     srcAddr,
            McastContentRcvr&       msgRcvr,
            const unsigned      version);

    /**
     * Constructs a source-specific multicast receiver.
     * @param[in] srcMcastInfo  Information on the source-specific multicast
     * @param[in] msgRcvr       Receiver of multicast messages. Must exist for
     *                          the duration of the constructed instance.
     * @param[in] version       Protocol version
     */
    McastReceiver(
            const SrcMcastInfo& srcMcastInfo,
            McastContentRcvr&       msgRcvr,
            const unsigned      version)
    	: McastReceiver(srcMcastInfo.mcastAddr, srcMcastInfo.srcAddr, msgRcvr,
    			version)
    {}

    /**
     * Runs the receiver. Receives multicast messages and forwards them to the
     * multicast message receiver. Doesn't return unless a zero-size datagram is
     * received (indicating end-of-session) or an exception is thrown.
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Compatible but not safe
     */
    void operator()();
};

} // namespace

#endif /* MAIN_MCAST_MCASTRECEIVER_H_ */
