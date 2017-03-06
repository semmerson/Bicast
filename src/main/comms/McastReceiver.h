/**
 * This file declares a receiver of multicast datagrams.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: McastReceiver.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_COMMS_MCASTRECEIVER_H_
#define MAIN_COMMS_MCASTRECEIVER_H_

#include "config.h"

#include <memory>

namespace hycast {

class McastReceiver
{
    class Impl;

    std::shared_ptr<Impl> pImpl;

public:
    McastReceiver(
            const InetSockAddr& mcastAddr,)

};

} // namespace

#endif /* MAIN_COMMS_MCASTRECEIVER_H_ */
