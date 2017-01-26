/**
 * This file declares a handle class for a receiver of multicast objects.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: McastRcvr.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_COMMS_MCASTRCVR_H_
#define MAIN_COMMS_MCASTRCVR_H_

#include "UdpSock.h"

#include <memory>

namespace hycast {

class McastRcvr
{
    class Impl; // Forward declaration of implementation

    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs.
     * @param[in] mcastSock  Multicast socket
     * @param[in] version    Protocol version
     */
    McastRcvr(
            McastUdpSock&  mcastSock,
            const unsigned version);
};

}

#endif /* MAIN_COMMS_MCASTRCVR_H_ */
