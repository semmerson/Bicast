/**
 * This file declares a modifiable set of potential peers.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PotentialPeers.h
 *  Created on: May 22, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_COMMS_P2P_POTENTIALPEERS_H_
#define MAIN_COMMS_P2P_POTENTIALPEERS_H_

#include "InetSockAddr.h"

#include <memory>

namespace hycast {

class PotentialPeers final
{
    class                           Impl;
    std::shared_ptr<PotentialPeers> pImpl;

public:
    PotentialPeers();
    ~PotentialPeers();
    void add(const InetSockAddr& srvrAddr);
};

} // namespace

#endif /* MAIN_COMMS_P2P_POTENTIALPEERS_H_ */
