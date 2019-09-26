/**
 * A thread-compatible but thread-unsafe queue of port-numbers.
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
     * @param[in] min  Minimum port number (inclusive) in host byte-order
     * @param[in] num  Number of port numbers
     */
    PortPool(
            in_port_t min,
            unsigned  num);

    /**
     * Returns the number of port-numbers in the queue.
     *
     * @return        Number of port numbers in the queue
     * @threadsafety  Compatible but unsafe
     */
    int size() const;

    /**
     * Removes port-number at the head of the queue.
     *
     * @return                   Port number at head of queue in host byte-order
     * @throws std::range_error  The queue is empty
     * @threadsafety             Compatible but unsafe
     */
    in_port_t take();

    /**
     * Adds a port-number to the end of the queue.
     *
     * @param[in] port  The port-number in host byte-order
     * @threadsafety    Compatible but unsafe
     */
    void add(in_port_t port);
};

} // namespace

#endif /* MAIN_PEER_PORTPOOL_H_ */
