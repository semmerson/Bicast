/**
 * This file declares the interface for a source of potential peers, which are
 * identified by the Internet socket address of their servers that listens for
 * connections by remote peers.
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

#include "InetSockAddr.h"

#include <iterator>
#include <memory>
#include <utility>

namespace hycast {

class PeerSource {
public:
    class iterator :
        public std::iterator<std::forward_iterator_tag, InetSockAddr>
    {
    public:
        virtual                     ~iterator();
        virtual bool                operator!=(const iterator& rhs);
        virtual iterator&           operator++();
        virtual const InetSockAddr& operator*();
    };

    virtual ~PeerSource();

    /**
     * Returns an iterator over the potential peers. Blocks if no peers are
     * available.
     * @return Iterator over potential peers:
     *   - First element:  Forward iterator
     *   - Second element: "End" iterator
     */
    virtual std::pair<iterator&,iterator&> getPeers() =0;
};

} // namespace

#endif /* MAIN_P2P_PEERSOURCE_H_ */
