/**
 * A thread-compatible but thread-unsafe pool of port-numbers.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PortSet.h
 *  Created on: Jul 13, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PEER_PORTPOOL_H_
#define MAIN_PEER_PORTPOOL_H_

#include <memory>
#include <netinet/in.h>

namespace hycast {

class PortPool
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    PortPool(Impl* const impl);

public:
    /**
     * Default constructs.
     */
    PortPool();

    /**
     * Constructs from a range of port-numbers.
     *
     * @param[in] min  Minimum port number (inclusive)
     * @param[in] max  Maximum port number (inclusive)
     */
    PortPool(
            in_port_t min,
            in_port_t max);

    /**
     * Returns the number of port-numbers in the pool.
     *
     * @return        Number of port numbers in the pool
     * @threadsafety  Compatible but unsafe
     */
    int size() const;

    /**
     * Removes the next port-number.
     *
     * @return                   Next port number
     * @throws std::range_error  The pool is empty
     * @threadsafety             Compatible but unsafe
     */
    in_port_t take();

    /**
     * Adds a port-number.
     *
     * @param[in] port  The port-number
     * @threadsafety    Compatible but unsafe
     */
    void add(in_port_t port);
};

} // namespace

#endif /* MAIN_PEER_PORTPOOL_H_ */
