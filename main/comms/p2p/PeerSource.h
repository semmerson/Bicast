/**
 * This file declares the interface for a source of potential peers. Each
 * potential peer is identified by the Internet socket address of its server.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerSource.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_P2P_PEERSOURCE_H_
#define MAIN_P2P_PEERSOURCE_H_

#include "InetSockAddr.h"

#include <chrono>

namespace hycast {

class PeerSource {
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    typedef std::chrono::seconds Duration;

    PeerSource();

    virtual ~PeerSource() =0;

    /**
     * Adds a potential peer.
     * @param[in] peerAddr  Internet socket address of the remote peer
     * @param[in] delay     The delay, in seconds, for the potential peer before
     *                      it becomes available
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Safe
     */
    void push(
            const InetSockAddr& peerAddr,
            const unsigned      delay = 0);

    /**
     * Returns the potential peer whose reveal-time is the earliest and not
     * later than the current time and removes it from the set of potential
     * peers. Blocks until such a value is available.
     * @return          The Internet socket address of the potential peer with
     *                  the earliest reveal-time that's not later than the
     *                  current time.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Safe
     */
    InetSockAddr pop();

    /**
     * Indicates if the source has any potential peers
     * @retval `true`   The source has potential peers
     * @retval `false`  The source has no potential peers
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    bool empty() const noexcept;
};

class NilPeerSource final : public PeerSource
{
public:
    inline void push(
            const InetSockAddr& peerAddr,
            const Duration&     delay = Duration(0))
    {};
    inline InetSockAddr pop()
    {
        return InetSockAddr{};
    }
    inline bool empty() const noexcept
    {
        return true;
    }
};

} // namespace

#endif /* MAIN_P2P_PEERSOURCE_H_ */
