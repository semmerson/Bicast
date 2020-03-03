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

#include <sender_receiver/Sender.h>
#include "config.h"
#include "error.h"
#include "McastProto.h"
#include "P2pMgr.h"

namespace hycast {

class Sender::Impl : public P2pMgrObs
{
    P2pMgr       p2pMgr;
    McastSndr    mcastSndr;
    SndRepo      repo;
    PeerChngObs& sndrObs;
    SegSize      segSize;

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

public:
    Impl(   P2pInfo&    p2pSrvrInfo,
            const SockAddr& grpAddr,
            SndRepo&        repo,
            PeerChngObs&    sndrObs)
        : p2pMgr{}
        , mcastSndr{UdpSock(grpAddr)}
        , repo{repo}
        , sndrObs(sndrObs)
        , segSize{repo.getSegSize()}
    {
        mcastSndr.setMcastIface(p2pSrvrInfo.sockAddr.getInetAddr());
        ServerPool srvrPool{}; // Empty pool => won't contact remote servers
        p2pMgr = P2pMgr(p2pSrvrInfo.sockAddr, p2pSrvrInfo.listenSize,
                p2pSrvrInfo.portPool, p2pSrvrInfo.maxPeers, srvrPool, *this);
    }

    void operator()()
    {
        try {
            p2pMgr();
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Failure"));
        }
    }

    void halt()
    {
        p2pMgr.halt();
    }

    void send(
            const std::string& prodName,
            const ProdIndex    prodIndex)
    {
        // Tell repository
        repo.newProd(prodName, prodIndex);

        // Send product-information
        auto prodInfo = repo.getProdInfo(prodIndex);
        send(prodInfo);

        // Send data-segments
        auto prodSize = prodInfo.getProdSize();
        for (ProdSize offset = 0; offset < prodSize; offset += segSize)
            send(repo.getMemSeg(SegId(prodIndex, offset)));
    }

    void added(Peer& peer)
    {
        sndrObs.added(peer);
    }

    void removed(Peer& peer)
    {
        sndrObs.removed(peer);
    }

    bool shouldRequest(ProdIndex prodIndex)
    {
        return false; // Meaningless for a sender
    }

    bool shouldRequest(const SegId& segId)
    {
        return false; // Meaningless for a sender
    }

    bool hereIsP2p(const ProdInfo& prodInfo)
    {
        return false; // Meaningless for a sender
    }

    bool hereIs(TcpSeg& tcpSeg)
    {
        return false; // Meaningless for a sender
    }

    ProdInfo get(ProdIndex prodIndex)
    {
        return repo.getProdInfo(prodIndex);
    }

    MemSeg get(const SegId& segId)
    {
        return repo.getMemSeg(segId);
    }
};

/******************************************************************************/

Sender::Sender(
        P2pInfo&    p2pSrvrInfo,
        const SockAddr& grpAddr,
        SndRepo&        repo,
        PeerChngObs&    sndrObs)
    : pImpl{new Impl(p2pSrvrInfo,  grpAddr, repo, sndrObs)}
{}

void Sender::operator()() const
{
    pImpl->operator()();
}

void Sender::halt() const
{
    pImpl->halt();
}

void Sender::send(
        const std::string& prodName,
        const ProdIndex    prodIndex) const
{
    pImpl->send(prodName, prodIndex);
}

} // namespace
