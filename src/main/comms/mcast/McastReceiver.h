/**
 * This file declares a receiver of multicast datagrams.
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
#include "../mcast/McastMsgRcvr.h"

namespace hycast {

class McastReceiver
{
    class Impl;

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
            McastMsgRcvr&       msgRcvr,
            const unsigned      version);

    /**
     * Constructs a source-dependent multicast receiver.
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
            McastMsgRcvr&       msgRcvr,
            const unsigned      version);

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
