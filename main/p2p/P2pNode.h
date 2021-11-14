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
#include "PeerSrvrAddrs.h"
#include "Repository.h"

#include <cstdint>
#include <memory>
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

    /**
     * Creates a publisher's P2P node.
     *
     * @param[in] pubNode       Publisher's Hycast node
     * @param[in] peerSrvrAddr  P2P server's socket address. It shall specify a
     *                          specific interface. The port number may be 0, in
     *                          which case the operating system will choose it.
     * @param[in] maxPeers      Maximum number of subscribing peers
     * @param[in] segSize       Size, in bytes, of canonical data-segment
     * @return                  Publisher's P2P node
     */
    static std::shared_ptr<P2pNode> create(Node&          pubNode,
                                           const SockAddr peerSrvrAddr,
                                           const unsigned maxPeers,
                                           const SegSize  segSize);

    virtual ~P2pNode() noexcept =default;

    /**
     * Indicates if this instance has a path to the publisher.
     *
     * @retval `true`   This instance has path to publisher
     * @retval `false`  This instance doesn't have path to publisher
     */
    virtual bool isPathToPub() const =0;

    /**
     * Notifies connected remote peers about the availability of product
     * information.
     *
     * @param[in] prodInfo  Product information
     */
    virtual void notify(const ProdIndex prodIndex) =0;

    /**
     * Notifies connected remote peers about the availability of a data
     * segment.
     *
     * @param[in] dataSegId  Data segment ID
     */
    virtual void notify(const DataSegId& dataSegId) =0;

    /**
     * Accepts being notified that a local peer has lost the connection with
     * its remote peer.
     *
     * @param[in] peer  Local peer
     */
    virtual void lostConnection(Peer peer) =0;

    /**
     * Receives a request for product information from a remote peer.
     *
     * @param[in] request      Which product
     * @param[in] peer         Associated local peer
     * @return                 Product information. Will test false if it
     *                         shouldn't be sent to remote peer.
     */
    virtual ProdInfo recvRequest(const ProdIndex request,
                                 Peer            peer) =0;
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
};

/// Interface for a subscribing P2P node
class SubP2pNode : virtual public P2pNode
                 , public NoticeRcvr
                 , public DataRcvr
{
public:
    /**
     * Creates a subscribing P2P node.
     *
     * @param[in] subNode          Subscriber's node
     * @param[in] pubPeerSrvrAddr  Socket address of publisher's peer-server
     * @param[in] peerSrvrAddrs    Socket addresses of potential peer-servers
     * @param[in] subPeerSrvrAddr  Socket address of subscriber's peer-server.
     *                             IP address *must not* specify all interfaces.
     *                             If port number is 0, then O/S will choose.
     * @param[in] maxPeers         Maximum number of peers. May be adjusted.
     * @param[in] segSize          Size, in bytes, of canonical data-segment
     * @return                     Subscribing P2P node
     */
    static std::shared_ptr<SubP2pNode> create(SubNode&       subNode,
                                              const SockAddr pubPeerSrvrAddr,
                                              PeerSrvrAddrs  peerSrvrAddrs,
                                              const SockAddr subPeerSrvrAddr,
                                              const unsigned maxPeers,
                                              const SegSize  segSize);

    virtual bool isPathToPub() const =0;

    /**
     * Accepts notification about whether or not a remote P2P node is a path to
     * the publisher.
     *
     * @param[in] notice  Whether or not remote node is path to publisher
     * @param[in] peer    Local peer that received the notice
     */
    virtual void recvNotice(const PubPath notice,
                            Peer          peer) =0;

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
    virtual DataSeg recvRequest(const DataSegId request,
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
    virtual void missed(const DataSegId dataSegId,
                        Peer            peer) =0;

    /**
     * Accepts a set of addresses of potential peer-servers.
     *
     * @param[in] peerSrvrAddrs  Addresses of potential peer-servers
     * @param[in] peer           Local peer that received the data
     */
    virtual void recvData(const PeerSrvrAddrs peerSrvrAddrs,
                          Peer                peer) =0;

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

    /**
     * Accepts being notified that a local peer has lost the connection with
     * its remote peer.
     *
     * @param[in] peer  Local peer
     */
    virtual void lostConnection(Peer peer) =0;
};

} // namespace

#endif /* MAIN_PROTO_P2PNODE_H_ */
