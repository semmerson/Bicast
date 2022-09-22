/**
 * This file implements the Hycast multicast component.
 *
 *  @file:  mcast.cpp
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */

#include "config.h"

#include "mcast.h"

#include "Node.h"
#include "Xprt.h"

namespace hycast {

/// Base class for multicast implementations
class McastImpl
{
protected:
    UdpSock sock;
    Xprt    xprt;

public:
    McastImpl(const UdpSock sock)
        : sock(sock)
        , xprt{sock}
    {}
};

/**************************************************************************************************/

/// Implementation of a multicast publisher
class McastPubImpl final : public McastImpl, public McastPub
{
    template<typename DATUM>
    void cast(const DATUM datum) {
        if (!xprt.write(DATUM::pduId) || !datum.write(xprt) || !xprt.flush())
            throw SYSTEM_ERROR("Multicast transport, " + xprt.to_string() + ", closed");
    }

public:
    McastPubImpl(
            const SockAddr mcastAddr,
            const InetAddr ifaceAddr)
        : McastImpl{UdpSock{mcastAddr, ifaceAddr}}
    {}

    void multicast(const ProdInfo prodInfo) {
        cast<ProdInfo>(prodInfo);
        LOG_DEBUG("Multicasted product information %s on transport %s",
                prodInfo.to_string().data(), xprt.to_string().data());
    }

    void multicast(const DataSeg dataSeg) {
        cast<DataSeg>(dataSeg);
        LOG_DEBUG("Multicasted data segment %s on transport %s",
                dataSeg.getId().to_string().data(), xprt.to_string().data());
    }
};

McastPub::Pimpl McastPub::create(
        const SockAddr mcastAddr,
        const InetAddr ifaceAddr) {
    return Pimpl(new McastPubImpl(mcastAddr, ifaceAddr));
}

/**************************************************************************************************/

/// Implementation of a multicast subscriber
class McastSubImpl final : public McastImpl, public McastSub
{
    SubNode& node;

    template<typename DATUM>
    void recvDatum() {
        DATUM datum;
        if (!datum.read(xprt))
            throw SYSTEM_ERROR("Multicast transport, " + xprt.to_string() + ", closed");
        LOG_TRACE("Received multicast datum %s on transport %s", datum.getId().to_string().data(),
                xprt.to_string().data());
        node.recvMcastData(datum);
    }

public:
    /**
     * Constructs.
     *
     * @param[in] mcastAddr        Address of multicast group
     * @param[in] srcAddr          IP address of publisher
     * @param[in] iface            IP address of interface to use. If wildcard, then O/S chooses.
     * @param[in] node             Subscribing node to call
     * @throw     InvalidArgument  Multicast group IP address isn't source-specific
     * @throw     LogicError       IP address families don't match
     */
    McastSubImpl(
            const SockAddr& mcastAddr,
            const InetAddr& srcAddr,
            const InetAddr& ifaceAddr,
            SubNode&        node)
        : McastImpl{UdpSock{mcastAddr, srcAddr, ifaceAddr}}
        , node(node)
    {}

    ~McastSubImpl() {
        halt();
    }

    void run() {
        LOG_DEBUG("Running multicast receiver using transport " + xprt.to_string());
        try {
            for (;;) {
                PduId pduId;
                if (!pduId.read(xprt))
                    break;

                if (pduId == PduId::PROD_INFO) {
                    recvDatum<ProdInfo>();
                }
                else if (pduId == PduId::DATA_SEG) {
                    recvDatum<DataSeg>();
                }
                else {
                    LOG_WARNING("Unknown PDU ID: %s", pduId.to_string().data());
                }

                xprt.clear();
            }
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Multicast error"));
        }
    }

    void halt() {
        xprt.shutdown();
    }
};

McastSub::Pimpl McastSub::create(
        const SockAddr& mcastAddr,
        const InetAddr& srcAddr,
        const InetAddr& iface,
        SubNode&        node) {
    return Pimpl(new McastSubImpl(mcastAddr, srcAddr, iface, node));
}

} // namespace
