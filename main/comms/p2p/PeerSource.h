/**
 * This file declares the interface for a source of potential peers, which are
 * identified by the Internet socket address of their servers that listen for
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

namespace hycast {

class PeerSource {
public:
    typedef const std::set<InetSockAddr>::iterator Iterator;

    virtual ~PeerSource() {};

    /**
     * Returns an iterator over the potential peers. Blocks if no peers are
     * available.
     * @return Iterator over potential peers
     */
    virtual Iterator getPeers() =0;

    virtual Iterator end() =0;
};

} // namespace

#endif /* MAIN_P2P_PEERSOURCE_H_ */
