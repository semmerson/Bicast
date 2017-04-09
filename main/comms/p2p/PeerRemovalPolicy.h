/**
 * This file declares the policy for removing a peer from a set of peers.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerRemovalPolicy.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_COMMS_P2P_PEERREMOVALPOLICY_H_
#define MAIN_COMMS_P2P_PEERREMOVALPOLICY_H_

#include "PeerSet.h"

namespace hycast {

class PeerRemovalPolicy final
{
public:
    typedef std::pair<Peer, uint32_t> Element;

    PeerRemovalPolicy(std::function<int(const Element&, const Element&)>);
    virtual ~PeerRemovalPolicy();
    Peer removeWorst(PeerSet& peerSet);
};

} // namespace

#endif /* MAIN_COMMS_P2P_PEERREMOVALPOLICY_H_ */
