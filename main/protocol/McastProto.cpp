/**
 * Multicast protocol.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: McastProto.cpp
 *  Created on: Nov 5, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "hycast.h"
#include "McastProto.h"
#include "protocol.h"

namespace hycast {

typedef uint16_t MsgIdType;

static MsgIdType prodInfoId = MsgId::PROD_INFO;
static MsgIdType dataSegId = MsgId::DATA_SEG;

class McastSndr::Impl {
    UdpSock sock;

public:
    Impl(UdpSock& sock)
        : sock{sock}
    {}

    Impl(UdpSock&& sock)
        : sock{sock}
    {}

    void setMcastIface(const InetAddr& interface)
    {
        sock.setMcastIface(interface);
    }

    void multicast(const ProdInfo& prodInfo)
    {
        LOG_DEBUG("Multicasting product-information %s on socket %s",
                prodInfo.to_string().data(), sock.to_string().data());

        sock.addWrite(prodInfoId);
        const std::string& name = prodInfo.getProdName();
        sock.addWrite(static_cast<SegSize>(name.length()));
        sock.addWrite(prodInfo.getProdIndex().getValue());
        sock.addWrite(prodInfo.getProdSize());
        sock.addWrite(name.data(), name.length());
        sock.write();
    }

    void multicast(const MemSeg& seg)
    {
        LOG_DEBUG("Multicasting data-segment %s on socket %s",
                seg.to_string().data(), sock.to_string().data());

        sock.addWrite(dataSegId);
        sock.addWrite(seg.getSegSize());
        sock.addWrite(seg.getProdIndex().getValue());
        sock.addWrite(seg.getProdSize());
        sock.addWrite(seg.getSegOffset());
        sock.addWrite(seg.data(), seg.getSegSize());
        sock.write();
    }
};

McastSndr::McastSndr(UdpSock& sock)
    : pImpl{new Impl(sock)}
{}

McastSndr::McastSndr(UdpSock&& sock)
    : pImpl{new Impl(sock)}
{}

const McastSndr& McastSndr::setMcastIface(const InetAddr& iface) const
{
    pImpl->setMcastIface(iface);
    return *this;
}

void McastSndr::multicast(const ProdInfo& info)
{
    pImpl->multicast(info);
}

void McastSndr::multicast(const MemSeg& seg)
{
    pImpl->multicast(seg);
}

/******************************************************************************/

class McastRcvr::Impl
{
    UdpSock          sock;
    McastSub*    mcastSub;

    void addCommon(
            SegSize&         varSize,
            ProdIndex::Type& prodIndex,
            ProdSize&        prodSize)
    {
        sock.addPeek(varSize);
        sock.addPeek(prodIndex);
        sock.addPeek(prodSize);
    }

    bool recvProdInfo()
    {
        LOG_DEBUG("Receiving product-information on socket %s",
                sock.to_string().data());

        SegSize         nameLen;
        ProdIndex::Type prodIndex;
        ProdSize        prodSize;
        addCommon(nameLen, prodIndex, prodSize);
        if (!sock.peek())
            return false;

        char buf[nameLen];
        sock.addPeek(buf, nameLen);
        if (!sock.peek())
            return false;

        sock.discard();

        mcastSub->hereIsMcast(ProdInfo{prodIndex, prodSize,
            std::string(buf, nameLen)});

        return true;
    }

    bool recvDataSeg()
    {
        LOG_DEBUG("Receiving data-segment on socket %s",
                sock.to_string().data());

        SegSize         segSize;
        ProdIndex::Type prodIndex;
        ProdSize        prodSize;
        ProdSize        segOffset;
        addCommon(segSize, prodIndex, prodSize);
        sock.addPeek(segOffset);
        if (!sock.peek())
            return false;

        UdpSeg udpSeg{SegInfo{SegId{prodIndex, segOffset}, prodSize, segSize},
                sock};
        mcastSub->hereIs(udpSeg);
        sock.discard();

        return true;
    }

public:
    Impl(   const SrcMcastAddrs& srcMcastInfo,
            McastSub&            mcastSub)
        : sock{srcMcastInfo.grpAddr, srcMcastInfo.srcAddr}
        , mcastSub{&mcastSub}
    {}

    /**
     * Returns on EOF.
     */
    void operator()()
    {
        try {
            for (;;) {
                MsgIdType msgId;
                sock.addPeek(msgId);
                if (!sock.peek())
                    break; // EOF or `sock.halt()` called

                if (msgId == MsgId::PROD_INFO) {
                    if (!recvProdInfo())
                        break;
                }
                else if (msgId == MsgId::DATA_SEG) {
                    if (!recvDataSeg())
                        break;
                }

                sock.discard();
            } // Indefinite loop
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(
                    RUNTIME_ERROR("Multicast reception failure"));
        }
    }

    /**
     * Causes `operator()()` to return.
     *
     * @throws    SystemError      `::shutdown()` failure
     */
    void halt()
    {
        sock.shutdown();
    }
};

McastRcvr::McastRcvr(
        const SrcMcastAddrs& srcMcastInfo,
        McastSub&            mcastSub)
    : pImpl{new Impl(srcMcastInfo, mcastSub)}
{}

void McastRcvr::operator()()
{
    pImpl->operator()();
}

void McastRcvr::halt()
{
    pImpl->halt();
}

} // namespace
