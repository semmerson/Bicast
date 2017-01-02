/**
 * This file declares a source of potential peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerSource.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_P2P_PEERSOURCE_H_
#define MAIN_P2P_PEERSOURCE_H_

#include "InetAddr.h"

#include <iterator>
#include <utility>

namespace hycast {

class PeerSource {
public:
    typedef std::iterator<std::forward_iterator_tag, InetAddr> iterator;

    virtual ~PeerSource();

    /**
     * Returns an iterator over the potential peers. Blocks if no peers are
     * available.
     * @return Iterator over potential peers. First element is the forward
     *         iterator; second element is the "end" iterator.
     */
    virtual std::pair<iterator,iterator> getPeers();
};

} // namespace

#endif /* MAIN_P2P_PEERSOURCE_H_ */
