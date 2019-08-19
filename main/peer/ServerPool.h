/**
 * Pool of potential servers for remote peers.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: ServerPool.h
 *  Created on: Jun 29, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PEER_SERVERPOOL_H_
#define MAIN_PEER_SERVERPOOL_H_

#include "SockAddr.h"

#include <memory>
#include <set>

namespace hycast {

class ServerPool
{
public:
    class Impl;

private:
    std::shared_ptr<Impl> pImpl;

    /**
     * Default constructs.
     */
    ServerPool();

    /**
     * Constructs from an implementation.
     *
     * @param[in] impl  The implementation
     */
    ServerPool(Impl* const impl);

public:
    /**
     * Constructs from a set of addresses of potential servers.
     *
     * @param[in] servers  Set of addresses of potential servers
     */
    ServerPool(const std::set<SockAddr>& servers);

    /**
     * Indicates if `pop()` will immediately return.
     *
     * @retval `true`   Yes
     * @retval `false`  No
     * @exceptionsafety No throw
     * @threadsafety    Safe
     */
    bool ready() const noexcept;

    /**
     * Returns the address of the next potential server for a remote peer.
     * Blocks until one can be returned.
     *
     * @return          Address of a potential server for a remote peer
     * @exceptionsafety No throw
     * @threadsafety    Safe
     */
    SockAddr pop() const noexcept;

    /**
     * Possibly returns the address of a server to the pool. There is no
     * guarantee that the address will be subsequently returned by `pop()`.
     *
     * @param[in] server  Address of server
     * @param[in] delay   Delay, in seconds, before the address could possibly
     *                    be returned by `pop()`
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void consider(
            const SockAddr& server,
            const unsigned  delay = 0) const;
};

} // namespace

#endif /* MAIN_PEER_SERVERPOOL_H_ */
