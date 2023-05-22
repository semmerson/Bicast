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
            LOG_TRACE("RPC " + xprt.to_string() + " received request for " + desc + " " +
                    id.to_string());
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
     * @retval    true    Success
     * @retval    false   Connection lost
     */
    template<class NOTICE>
    inline bool processNotice(
            Xprt&             xprt,
            Peer&             peer,
            const char* const desc) {
        NOTICE notice;
        if (!notice.read(xprt))
            return false;

        LOG_TRACE("RPC " + xprt.to_string() + " received notice about " + desc + " " +
                notice.to_string());
        peer.recvNotice(notice);
        return true;
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
            LOG_TRACE("RPC " + xprt.to_string() + " received " + desc + " " + datum.to_string());
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
        bool connected = false;

        switch (pduId) {
            // Notices:

            case PduId::SET_RMT_SRVR_INFO: {
                P2pSrvrInfo srvrInfo;
                if (srvrInfo.read(xprt)) {
                    connected = true;
                    LOG_DEBUG("RPC " + xprt.to_string() + " received P2P-server information " +
                            srvrInfo.to_string());
                    peer.setRmtSrvrInfo(srvrInfo);
                }
                break;
            }
            case PduId::DATA_SEG_NOTICE: {
                connected = processNotice<DataSegId>(xprt, peer, "data-segment");
                break;
            }
            case PduId::PROD_INFO_NOTICE: {
                connected = processNotice<ProdId>(xprt, peer, "product");
                break;
            }
            case PduId::PEER_SRVR_INFOS: {
                Tracker tracker;
                if (tracker.read(xprt)) {
                    connected = true;
                    LOG_DEBUG("RPC " + xprt.to_string() + " received tracker " +
                            tracker.to_string());
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

    bool setRmtSrvrInfo(
            Xprt&              xprt,
            const P2pSrvrInfo& srvrInfo) override {
        const auto success = send(xprt, PduId::SET_RMT_SRVR_INFO, srvrInfo);
        if (success)
            LOG_DEBUG("RPC " + xprt.to_string() + " sent local P2P-server info " +
                    srvrInfo.to_string());
        return success;
    }

    bool add(
            Xprt&          xprt,
            const Tracker& tracker) override {
        const auto success = send(xprt, PduId::PEER_SRVR_INFOS, tracker);
        if (success)
            LOG_DEBUG("RPC " + xprt.to_string() + " sent tracker " + tracker.to_string());
        return success;
    }

    bool notify(
            Xprt&        xprt,
            const ProdId prodId) override {
        const auto success = send(xprt, PduId::PROD_INFO_NOTICE, prodId);
        if (success)
            LOG_TRACE("RPC sent product index " + prodId.to_string());
        return success;
    }
    bool notify(
            Xprt&           xprt,
            const DataSegId dataSegId) override {
        const auto success = send(xprt, PduId::DATA_SEG_NOTICE, dataSegId);
        if (success)
            LOG_TRACE("RPC sent data segment ID " + dataSegId.to_string());
        return success;
    }

    // Requests:

    bool request(
            Xprt&        xprt,
            const ProdId prodId) override {
        const auto success = send(xprt, PduId::PROD_INFO_REQUEST, prodId);
        if (success)
            LOG_TRACE("RPC requested information on product " + prodId.to_string());
        return success;
    }
    bool request(
            Xprt&           xprt,
            const DataSegId dataSegId) override {
        const auto success = send(xprt, PduId::DATA_SEG_REQUEST, dataSegId);
        if (success)
            LOG_TRACE("RPC requested data segment " +  dataSegId.to_string());
        return success;
    }

    bool request(
            Xprt&            xprt,
            const ProdIdSet& prodIds) override {
        const auto success = send(xprt, PduId::PREVIOUSLY_RECEIVED, prodIds);
        if (success)
            LOG_TRACE("RPC sent product IDs " +  prodIds.to_string());
        return success;
    }

    // Data:

    bool send(
            Xprt&          xprt,
            const ProdInfo prodInfo) override {
        const auto success = send(xprt, PduId::PROD_INFO, prodInfo);
        if (success)
            LOG_TRACE("RPC sent product information " + prodInfo.to_string());
        return success;
    }

    bool send(
            Xprt&         xprt,
            const DataSeg dataSeg) override {
        const auto success = send(xprt, PduId::DATA_SEG, dataSeg);
        if (success)
            LOG_TRACE("RPC sent data segment " +  dataSeg.to_string());
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
    return Pimpl{new RpcImpl{}};
}

} // namespace
