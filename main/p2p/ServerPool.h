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

public:
    /**
     * Default constructs. The pool will be empty.
     */
    ServerPool();

    /**
     * Constructs from a set of addresses of potential servers.
     *
     * @param[in] servers  Set of addresses of potential servers
     * @param[in] delay    Delay, in seconds, before a server given to
     *                     `consider()` is made available
     */
    ServerPool(const std::set<SockAddr>& servers, const unsigned delay = 60);

    /**
     * Constructs from the maximum number of socket addresses to contain.
     *
     * @param[in] maxServers  Maximum number of server socket addresses to
     *                        contain.
    ServerPool(const unsigned maxServers);
     */

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
     * @return                       Address of a potential server for a remote
     *                               peer
     * @throws    std::domain_error  `close()` was called.
     * @exceptionsafety              Strong guarantee
     * @threadsafety                 Safe
     * @cancellationpoint
     */
    SockAddr pop() const;

    /**
     * Possibly returns the address of a server to the pool. There is no
     * guarantee that the address will be subsequently returned by `pop()`.
     *
     * @param[in] server              Address of server
     * @param[in] delay               Delay, in seconds, before the address
     *                                could possibly be returned by `pop()`
     * @throws    std::domain_error  `close()` was called.
     * @exceptionsafety              Strong guarantee
     * @threadsafety                 Safe
     */
    void consider(SockAddr& server) const;

    /**
     * Closes the pool of servers. Causes `pop()` and `consider()` to throw an
     * exception. Idempotent.
     */
    void close();

    /**
     * Indicates if the pool of servers is empty. Even if false, `pop()` might
     * not immediately return.
     *
     * @retval `false`  Pool isn't empty
     * @retval `true`   Pool is empty
     */
    bool empty() const;
};

} // namespace

#endif /* MAIN_PEER_SERVERPOOL_H_ */
