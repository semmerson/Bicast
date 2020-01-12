/**
 * Transmitts data-products via the Hycast protocol.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Sender.cpp
 *  Created on: Jan 3, 2020
 *      Author: Steven R. Emmerson
 */

#include "config.h"
#include "Sender.h"

#include "error.h"
#include "McastProto.h"
#include "P2pMgr.h"

namespace hycast {

class Sender::Impl : public P2pMgrObs
{
    P2pMgr      p2pMgr;
    McastSndr   mcastSndr;
    SrcRepo     repo;

public:
    Impl(   const SockAddr& srvrAddr,
            const int       listenSize,
            PortPool&       portPool,
            const int       maxPeers,
            const SockAddr& grpAddr,
            SrcRepo&        repo)
        : p2pMgr{}
        , mcastSndr{UdpSock(grpAddr)}
        , repo{repo}
    {
        mcastSndr.setMcastIface(srvrAddr.getInetAddr());
        ServerPool srvrPool{}; // Empty pool => won't contact remote servers
        p2pMgr = P2pMgr(srvrAddr, listenSize, portPool, maxPeers, srvrPool,
                *this);
    }

    void added(Peer& peer)
    {
        LOG_INFO("Added peer %s", peer.to_string().data());
    }

    void removed(Peer& peer)
    {
        LOG_INFO("Removed peer %s", peer.to_string().data());
    }

    void send(const ProdInfo& prodInfo)
    {
        mcastSndr.multicast(prodInfo);
        p2pMgr.notify(prodInfo.getProdIndex());
    }

    void send(const MemSeg& memSeg)
    {
        mcastSndr.multicast(memSeg);
        p2pMgr.notify(memSeg.getSegId());
    }

    const OutChunk get(const ChunkId& chunkId)
    {
        return chunkId.get(repo);
    }
};

/******************************************************************************/

Sender::Sender(
        const SockAddr& srvrAddr,
        const int       listenSize,
        PortPool&       portPool,
        const int       maxPeers,
        const SockAddr& grpAddr,
        SrcRepo&        repo)
    : pImpl{new Impl(srvrAddr, listenSize, portPool, maxPeers,  grpAddr, repo)}
{}

void Sender::send(const ProdInfo& prodInfo) const
{
    pImpl->send(prodInfo);
}

void Sender::send(const MemSeg& memSeg) const
{
    pImpl->send(memSeg);
}

} // namespace
