/**
 * Multicast protocol.
 *
 *        File: McastProto.cpp
 *  Created on: Nov 5, 2019
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"

#include "error.h"
#include "Mcast.h"

namespace hycast {

using ProdInfoId = PduId::PROD_INFO;
using DataSegId = PduId::DATA_SEG;

class McastSndr::Impl {
    UdpSock sock;

public:
    Impl(UdpSock& sock)
        : sock(sock)
    {}

    Impl(UdpSock&& sock)
        : sock(sock)
    {}

    void setMcastIface(const InetAddr& interface)
    {
        sock.setMcastIface(interface);
    }

    void multicast(const ProdInfo& prodInfo)
    {
        LOG_DEBUG("Multicasting product-information " + prodInfo.to_string());

        try {
            sock.addWrite(static_cast<PduType>(PduId::PROD_INFO));
            prodInfo.write(sock);
            sock.write();
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't multicast "
                    "product-information " + prodInfo.to_string()));
        }
    }

    /**
     * @param[in] seg           Data-segment
     * @throws    RuntimeError  Couldn't multicast data-segment
     */
    void multicast(const MemSeg& seg)
    {
        LOG_DEBUG("Multicasting data-segment " + seg.getSegId().to_string());

        try {
            sock.addWrite(DataSegId);
            sock.addWrite(seg.getSegSize());
            sock.addWrite(seg.getProdIndex().getValue());
            sock.addWrite(seg.getProdSize());
            sock.addWrite(seg.getSegOffset());
            sock.addWrite(seg.data(), seg.getSegSize());
            sock.write();
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Exception thrown: %s", ex.what());
            std::throw_with_nested(RUNTIME_ERROR("Couldn't multicast "
                    "data-segment " + seg.getSegId().to_string()));
        }
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
        mcastSub->hereIsMcast(udpSeg);
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
