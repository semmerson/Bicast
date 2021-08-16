/**
 * This file declares the interface for a peer-to-peer node. Such a node
 * is called by peers to handle received PDU-s.
 * 
 * @file:   P2pNode.h
 * @author: Steven R. Emmerson
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

#ifndef MAIN_PROTO_P2PNODE_H_
#define MAIN_PROTO_P2PNODE_H_

#include "error.h"
#include "HycastProto.h"

#include <cstdint>
#include <string>

namespace hycast {

class Peer; // Forward declaration

/// Interface for a peer-to-peer node
class P2pNode : public RequestRcvr
{
public:
    /**
     * Relationship to the data-products:
     */
    enum class Type : char {
        UNSET,
        PUBLISHER,  // Node is the publisher
        SUBSCRIBER  // Node is a subscriber
    };

    virtual ~P2pNode() {}

    /**
     * Indicates if this instance has a path to the publisher.
     *
     * @retval `true`   This instance has path to publisher
     * @retval `false`  This instance doesn't have path to publisher
     */
    virtual bool isPathToPub() const =0;

    /**
     * Accepts being notified that a local peer has lost the connection with
     * its remote peer.
     *
     * @param[in] peer  Local peer
     */
    virtual void lostConnection(Peer peer) =0;

#if 0
    /**
     * Receives a request for product information from a remote peer.
     *
     * @param[in] request      Which product
     * @param[in] peer         Associated local peer
     * @return                 Product information. Will test false if it
     *                         shouldn't be sent to remote peer.
     */
    virtual ProdInfo recvRequest(const ProdIndex  request,
                                 Peer             peer) =0;
    /**
     * Receives a request for a data-segment from a remote peer.
     *
     * @param[in] request      Which data-segment
     * @param[in] peer         Associated local peer
     * @return                 Product information. Will test false if it
     *                         shouldn't be sent to remote peer.
     */
    virtual DataSeg  recvRequest(const DataSegId request,
                                 Peer            peer) =0;
#endif
};

/// Interface for a publishing P2P node
class PubP2pNode : public P2pNode
{
public:
    virtual ~PubP2pNode() {}

#if 0
    bool isPathToPub() const {
        return true;
    }

    virtual ProdInfo recvRequest(const ProdIndex request,
                                 Peer            peer) =0;

    virtual DataSeg  recvRequest(const DataSegId request,
                                 Peer            peer) =0;
#endif
};

/// Interface for a subscribing P2P node
class SubP2pNode : public PubP2pNode
                 , public NoticeRcvr
                 , public DataRcvr
{
public:
    virtual ~SubP2pNode() {}

    virtual bool isPathToPub() const =0;

    virtual void recvNotice(const PubPath    notice,
                            Peer             peer) =0;
    /**
     * Receives a notice of available product information from a remote peer.
     *
     * @param[in] notice       Which product
     * @param[in] peer         Associated local peer
     * @retval    `false`      Local peer shouldn't request from remote peer
     * @retval    `true`       Local peer should request from remote peer
     */
    virtual bool recvNotice(const ProdIndex  notice,
                            Peer             peer) =0;
    /**
     * Receives a notice of an available data-segment from a remote peer.
     *
     * @param[in] notice       Which data-segment
     * @param[in] peer         Associated local peer
     * @retval    `false`      Local peer shouldn't request from remote peer
     * @retval    `true`       Local peer should request from remote peer
     */
    virtual bool recvNotice(const DataSegId notice,
                            Peer            peer) =0;

    /**
     * Receives a request for product information from a remote peer.
     *
     * @param[in] request      Which product
     * @param[in] peer         Associated local peer
     * @return                 Product information. Will test false if it
     *                         shouldn't be sent to remote peer.
     */
    virtual ProdInfo recvRequest(const ProdIndex  request,
                                 Peer             peer) =0;
    /**
     * Receives a request for a data-segment from a remote peer.
     *
     * @param[in] request      Which data-segment
     * @param[in] peer         Associated local peer
     * @return                 Product information. Will test false if it
     *                         shouldn't be sent to remote peer.
     */
    virtual DataSeg  recvRequest(const DataSegId request,
                                 Peer            peer) =0;

    /**
     * Handles a request for data-product information not being satisfied by a
     * remote peer.
     *
     * @param[in] prodIndex  Index of the data-product
     * @param[in] peer       Local peer whose remote counterpart couldn't
     *                       satisfy request
     */
    virtual void missed(const ProdIndex prodIndex, Peer peer) =0;

    /**
     * Handles a request for a data-segment not being satisfied by a remote
     * peer.
     *
     * @param[in] dataSegId  ID of data-segment
     * @param[in] peer       Local peer whose remote counterpart couldn't
     *                       satisfy request
     */
    virtual void missed(const DataSegId& dataSegId, Peer peer) =0;

    /**
     * Accepts product information from a remote peer.
     *
     * @param[in] prodInfo  Product information
     * @param[in] peer      Associated local peer
     */
    virtual void recvData(const ProdInfo prodInfo,
                          Peer           peer) =0;

    /**
     * Accepts a data segment from a remote peer.
     *
     * @param[in] dataSeg  Data segment
     * @param[in] peer     Associated local peer
     */
    virtual void recvData(const DataSeg dataSeg,
                          Peer          peer) =0;
};

} // namespace

#endif /* MAIN_PROTO_P2PNODE_H_ */
