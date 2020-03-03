/**
 * Sends data-product via Hycast.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Sender.h
 *  Created on: Jan 3, 2020
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_SENDER_RECEIVER_SENDER_H_
#define MAIN_SENDER_RECEIVER_SENDER_H_

#include "Socket.h"
#include "P2pMgr.h"
#include "Repository.h"

#include <memory>

namespace hycast {

class Sender
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    Sender(Impl* const impl);

public:
    /**
     * Constructs.
     *
     * @param[in] p2pSrvrInfo  Information on the P2P server
     * @param[in] grpAddr      Address of multicast group for multicasting
     *                         products
     * @param[in] repo         Repository of transitory data-products to be sent
     * @param[in] sndrObs      Observer of this instance
     */
    Sender(
        P2pInfo&        p2pSrvrInfo,
        const SockAddr& grpAddr,
        SndRepo&        repo,
        PeerChngObs&    sndrObs);

    void operator()() const;

    void halt() const;

    void send(
            const std::string& prodName,
            ProdIndex          prodIndex) const;
};

} // namespace

#endif /* MAIN_SENDER_RECEIVER_SENDER_H_ */
