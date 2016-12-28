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

namespace hycast {

class PeerSource {
public:
    /**
     * Returns the next potential peer. Blocks until it's available.
     * @return Internet address of the next potential peer
     */
    InetAddr getNext();
};

} // namespace

#endif /* MAIN_P2P_PEERSOURCE_H_ */
