/**
 * This file declares a handle class for a receiver and sender of multicast
 * objects.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Multicaster.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_COMMS_MULTICASTER_H_
#define MAIN_COMMS_MULTICASTER_H_

#include "MsgRcvr.h"
#include "UdpSock.h"

#include <memory>

namespace hycast {

class Multicaster
{
    class Impl; // Forward declaration of implementation

    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs.
     * @param[in] mcastSock  Multicast socket
     * @param[in] version    Protocol version
     * @param[in] msgRcvr    Receiver of multicast objects or `nullptr`. If
     *                       non-null, then must exist for the duration of the
     *                       constructed instance.
     */
    Multicaster(
            McastUdpSock&  mcastSock,
            const unsigned version,
            MsgRcvr*       msgRcvr);
};

}

#endif /* MAIN_COMMS_MULTICASTER_H_ */
