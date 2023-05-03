/**
 * This file implements a P2P RPC layer.
 *
 *  @file:  Rpc.cpp
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
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

#include "Rpc.h"

#include "error.h"
#include "HycastProto.h"
#include "Peer.h"

#include <list>
#include <memory>
#include <poll.h>
#include <queue>
#include <unordered_map>

namespace hycast {

using XprtArray = std::array<Xprt, 3>; ///< Type of transport array for a connection to a peer

/**
 * Implements a P2P RPC layer.
 */
class RpcImpl final : public Rpc
{
#if 0
    /**
     * Vets the protocol version used by the remote RPC layer. Called by constructor.
     *
     * @param[in] protoVers  Remote protocol version
     * @throw LogicError     Remote RPC layer uses unsupported protocol
     */
    void vetProtoVers(decltype(PROTOCOL_VERSION) protoVers) {
        if (protoVers != PROTOCOL_VERSION)
            throw LOGIC_ERROR("RPC layer " + to_string() +
                    ": received incompatible protocol version " + std::to_string(protoVers) +
                    "; not " + std::to_string(PROTOCOL_VERSION));
    }

    /**
     * Sends then receives the protocol version and vets it. Called by constructor.
     */
    void sendAndVetProtoVers() {
        auto rmtProtoVers = PROTOCOL_VERSION;

        if (!noticeXprt.write(PROTOCOL_VERSION) || !noticeXprt.flush())
            throw RUNTIME_ERROR("Couldn't write to " + noticeXprt.to_string());
        if (!noticeXprt.read(rmtProtoVers))
            throw RUNTIME_ERROR("Couldn't read from " + noticeXprt.to_string());
        noticeXprt.clear();
        vetProtoVers(rmtProtoVers);
    }

    /**
     * Receives then sends the protocol version and vets it. Called by constructor.
     */
    void recvAndVetProtoVers() {
        auto rmtProtoVers = PROTOCOL_VERSION;

        if (!noticeXprt.read(rmtProtoVers))
            throw RUNTIME_ERROR("Couldn't read from " + noticeXprt.to_string());
        if (!noticeXprt.write(PROTOCOL_VERSION) || !noticeXprt.flush())
            throw RUNTIME_ERROR("Couldn't write to " + noticeXprt.to_string());
        noticeXprt.clear();
        vetProtoVers(rmtProtoVers);
    }

    /**
     * Tells the remote RPC layer if this instance is the publisher. Executed
     * by a server-side RPC layer only. Called by constructor.
     */
    inline void sendIsPub() {
        if (!noticeXprt.write(iAmPub) || !noticeXprt.flush())
            throw RUNTIME_ERROR("Couldn't write to " + noticeXprt.to_string());
    }

    /**
     * Receives from the remote RPC layer if that instance is the publisher.
     * Executed by a client-side RPC layer only. Called by constructor.
     */
    inline void recvIsPub() {
        if (!noticeXprt.read(rmtIsPub))
            throw RUNTIME_ERROR("Couldn't read from " + noticeXprt.to_string());
        noticeXprt.clear();
    }
#endif

    /**
     * Receives a request for a datum from the remote peer. Passes the request
     * to the associated local peer.
     *
     * @tparam    ID       Identifier of requested data
     * @param[in] xprt     Transport
     * @param[in] peer     Associated local peer
     * @param[in] desc     Description of datum
     * @retval    true     Success
     * @retval    false    Connection lost
     */
    template<class ID>
    inline bool processRequest(
            Xprt&             xprt,
            Peer&             peer,
            const char* const desc) {
        ID   id;
        bool success = id.read(xprt);
        if (success) {
            LOG_TRACE("RPC %s received request for %s %s",
                    xprt.to_string().data(), desc, id.to_string().data());
            peer.recvRequest(id);
        }
        return success;
    }

    /**
     * Processes notices of an available datum. Calls the associated peer.
     *
     * @tparam    NOTICE  Type of notice
     * @param[in] xprt    Transport
     * @param[in] peer    Associated peer
     * @param[in] desc    Description of associated datum
     */
    template<class NOTICE>
    inline void processNotice(
            Xprt&             xprt,
            Peer&             peer,
            const char* const desc) {
        NOTICE notice;
        if (notice.read(xprt)) {
            LOG_TRACE("RPC %s received notice about %s %s",
                    xprt.to_string().data(), desc, notice.to_string().data());
            peer.recvNotice(notice);
        }
    }

    /**
     * Processes a datum from the remote peer. Passes the datum to the associated peer.
     *
     * @tparam    DATUM       Type of datum (`ProdInfo`, `DataSeg`)
     * @param[in] xprt        Transport
     * @param[in] peer        Associated peer
     * @param[in] desc        Description of datum
     * @throw     LogicError  Datum wasn't requested
     * @see `Request::missed()`
     */
    template<class DATUM>
    inline bool processData(
            Xprt&             xprt,
            Peer&             peer,
            const char* const desc) {
        DATUM datum{};
        auto  success = datum.read(xprt);
        if (success) {
            LOG_TRACE("RPC %s received %s %s",
                    xprt.to_string().data(), desc, datum.to_string().data());
            peer.recvData(datum);
        }
        return success;
    }

    /**
     * Sends a PDU with no payload.
     *
     * @param[in] xprt     Transport to use
     * @param[in] pduId    PDU ID
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    inline bool send(
            Xprt            xprt,
            const PduId     pduId) {
        return pduId.write(xprt) && xprt.flush();
    }

    /**
     * Sends a transportable object as a PDU.
     *
     * @param[in] xprt     Transport to use
     * @param[in] pduId    PDU ID
     * @param[in] obj      Object to be sent
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    inline bool send(
            Xprt            xprt,
            const PduId     pduId,
            const XprtAble& obj) {
        return pduId.write(xprt) && obj.write(xprt) && xprt.flush();
    }

    /**
     * Dispatches incoming RPC messages by reading the payload and calling a peer.
     * @param[in] pduId  Protocol data unit identifier
     * @param[in] xprt   Transport
     * @param[in] peer   Associated peer
     * @retval    true   Success
     * @retval    false  Connection lost
     * @threadsafety     Safe
     */
    bool dispatch(
            const PduId pduId,
            Xprt&       xprt,
            Peer&       peer) {
        bool connected;

        switch (pduId) {
            // Notices:

            case PduId::DATA_SEG_NOTICE: {
                processNotice<DataSegId>(xprt, peer, "data-segment");
                break;
            }
            case PduId::PROD_INFO_NOTICE: {
                processNotice<ProdId>(xprt, peer, "product");
                break;
            }
            case PduId::PEER_SRVR_INFO: {
                P2pSrvrInfo srvrInfo;
                if (srvrInfo.read(xprt)) {
                    LOG_TRACE("RPC %s received notice about P2P server %s",
                            xprt.to_string().data(), srvrInfo.to_string().data());
                    peer.recvAdd(srvrInfo);
                }
                break;
            }
            case PduId::PEER_SRVR_INFOS: {
                Tracker tracker;
                if (tracker.read(xprt)) {
                    LOG_TRACE("RPC %s received notice about P2P servers %s",
                            xprt.to_string().data(), tracker.to_string().data());
                    peer.recvAdd(tracker);
                }
                break;
            }
            case PduId::PREVIOUSLY_RECEIVED: {
                ProdIdSet prodIds{0};
                auto connected = prodIds.read(xprt);
                if (connected)
                    peer.recvHaveProds(prodIds);
                break;
            }

            // Requests:

            case PduId::DATA_SEG_REQUEST: {
                connected = processRequest<DataSegId>(xprt, peer, "data-segment");
                break;
            }
            case PduId::PROD_INFO_REQUEST: {
                connected = processRequest<ProdId>(xprt, peer, "information on product");
                break;
            }

            // Data:

            case PduId::DATA_SEG: {
                connected = processData<DataSeg>(xprt, peer, "data segment");
                break;
            }
            case PduId::PROD_INFO: {
                connected = processData<ProdInfo>(xprt, peer, "product information");
                break;
            }

            default:
                LOG_WARNING("Unknown PDU ID: " + std::to_string(pduId));
                connected = true;
        }

        return connected;
    }

public:
    // Notices:

    bool add(
            Xprt&              xprt,
            const P2pSrvrInfo& srvrInfo) override {
        const auto success = send(xprt, PduId::PEER_SRVR_INFO, srvrInfo);
        if (success)
            LOG_TRACE("RPC sent P2P-server info %s", srvrInfo.to_string().data());
        return success;
    }

    bool add(
            Xprt&          xprt,
            const Tracker& tracker) override {
        const auto success = send(xprt, PduId::PEER_SRVR_INFOS, tracker);
        if (success)
            LOG_TRACE("RPC sent tracker %s", tracker.to_string().data());
        return success;
    }

    bool notify(
            Xprt&        xprt,
            const ProdId prodId) override {
        const auto success = send(xprt, PduId::PROD_INFO_NOTICE, prodId);
        if (success)
            LOG_TRACE("RPC sent product index %s", prodId.to_string().data());
        return success;
    }
    bool notify(
            Xprt&           xprt,
            const DataSegId dataSegId) override {
        const auto success = send(xprt, PduId::DATA_SEG_NOTICE, dataSegId);
        if (success)
            LOG_TRACE("RPC sent data segment ID %s", dataSegId.to_string().data());
        return success;
    }

    // Requests:

    bool request(
            Xprt&        xprt,
            const ProdId prodId) override {
        const auto success = send(xprt, PduId::PROD_INFO_REQUEST, prodId);
        if (success)
            LOG_TRACE("RPC requested information on product %s", prodId.to_string().data());
        return success;
    }
    bool request(
            Xprt&           xprt,
            const DataSegId dataSegId) override {
        const auto success = send(xprt, PduId::DATA_SEG_REQUEST, dataSegId);
        if (success)
            LOG_TRACE("RPC requested data segment %s",  dataSegId.to_string().data());
        return success;
    }

    bool request(
            Xprt&            xprt,
            const ProdIdSet& prodIds) override {
        const auto success = send(xprt, PduId::PREVIOUSLY_RECEIVED, prodIds);
        if (success)
            LOG_TRACE("RPC sent product IDs %s",  prodIds.to_string().data());
        return success;
    }

    // Data:

    bool send(
            Xprt&          xprt,
            const ProdInfo prodInfo) override {
        const auto success = send(xprt, PduId::PROD_INFO, prodInfo);
        if (success)
            LOG_TRACE("RPC sent product information %s", prodInfo.to_string().data());
        return success;
    }

    bool send(
            Xprt&         xprt,
            const DataSeg dataSeg) override {
        const auto success = send(xprt, PduId::DATA_SEG, dataSeg);
        if (success)
            LOG_TRACE("RPC sent data segment %s",  dataSeg.to_string().data());
        return success;
    }

    bool process(
            Xprt& xprt,
            Peer& peer) override {
        PduId pduId{};
        return pduId.read(xprt) && dispatch(pduId, xprt, peer);
    }
};

Rpc::Pimpl Rpc::create() {
    return Pimpl{};
}

Rpc::Pimpl Rpc::create(
        const SockAddr srvrAddr,
        const int      timeout) {
    return Pimpl{new RpcImpl};
}

} // namespace
