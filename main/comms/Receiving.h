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

#include "McastReceiver.h"
#include "McastMsgRcvr.h"
#include "P2pMgr.h"
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
     * @param[in] srcMcastInfo  Information on the source-specific multicast
     * @param[in] p2pInfo       Information for the peer-to-peer component
     * @param[in] processing    Locally processes received data-products
     * @param[in] version       Protocol version
     * @param[in] pathname      Pathname of product-store persistence-file or
     *                          the empty string to indicate no persistence
     * @see ProdStore::ProdStore()
     */
    Receiving(
            const SrcMcastInfo& srcMcastInfo,
            const P2pInfo&      p2pInfo,
            Processing&         processing,
            const unsigned      version,
            const std::string&  pathname = "");

    void recvNotice(const ProdInfo& info) const;

    void recvData(LatentChunk chunk) const;

    void recvNotice(const ProdInfo& info, const Peer& peer) const;

    void recvNotice(const ChunkInfo& info, const Peer& peer) const;

    void recvRequest(const ProdIndex& index, const Peer& peer) const;

    void recvRequest(const ChunkInfo& info, const Peer& peer) const;

    void recvData(LatentChunk chunk, const Peer& peer) const;
};

} /* namespace hycast */

#endif /* MAIN_RECEIVING_RECEIVING_H_ */
