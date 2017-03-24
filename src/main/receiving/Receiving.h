/**
 * This file declares a component that coordinates the reception of
 * data-products. Data-products are received in pieces (product-information,
 * chunks of data) via both multicast and peer-to-peer transports.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Receiving.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_RECEIVING_RECEIVING_H_
#define MAIN_RECEIVING_RECEIVING_H_

#include "McastMsgRcvr.h"
#include "PeerMsgRcvr.h"

#include <memory>
#include <string>

namespace hycast {

class Receiving final : public McastMsgRcvr, public PeerMsgRcvr
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs.
     * @param[in] pathname  Pathname of product-store persistence-file or the
     *                      empty string to indicate no persistence
     * @param[in] p2pMgr    Peer-to-peer manager
     * @see ProdStore::ProdStore()
     */
    Receiving(const std::string pathname, P2pMgr& p2pMgr);

    void recvNotice(const ProdInfo& info);

    void recvData(LatentChunk chunk);

    void recvNotice(const ProdInfo& info, Peer& peer);

    void recvNotice(const ChunkInfo& info, Peer& peer);

    void recvRequest(const ProdIndex& index, Peer& peer);

    void recvRequest(const ChunkInfo& info, Peer& peer);

    void recvData(LatentChunk chunk, Peer& peer);
};

} /* namespace hycast */

#endif /* MAIN_RECEIVING_RECEIVING_H_ */
