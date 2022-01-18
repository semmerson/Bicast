/**
 * This file declares the interface for a peer-to-peer manager. Such a manager
 * is called by peers to handle received PDU-s.
 * 
 * @file:   P2pMgr.h
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
#include "Repository.h"
#include "Tracker.h"

#include <cstdint>
#include <memory>
#include <string>

namespace hycast {

class Peer; // Forward declaration

/**
 * Interface for a peer-to-peer manager. A publisher's P2P manager will only
 * implement this interface.
 */
class P2pMgr : public RequestRcvr
{
public:
    using Pimpl = std::shared_ptr<P2pMgr>;

    /**
     * Relationship to the data-products:
     */
    enum class Type : char {
        UNSET,
        PUBLISHER,  // Publisher's P2P manager
        SUBSCRIBER  // Subscriber's P2P manager
    };

    /**
     * Creates a publisher's P2P manager.
     *
     * @param[in] pubNode       Publisher's Hycast node
     * @param[in] peerSrvrAddr  P2P server's socket address. It shall specify a
     *                          specific interface. The port number may be 0, in
     *                          which case the operating system will choose it.
     * @param[in] maxPeers      Maximum number of subscribing peers
     * @param[in] segSize       Size, in bytes, of canonical data-segment
     * @return                  Publisher's P2P manager
     */
    static Pimpl create(
            Node&          pubNode,
            const SockAddr peerSrvrAddr,
            unsigned       maxPeers,
            const SegSize  segSize);

    virtual ~P2pMgr() noexcept =default;

    /**
     * Returns the address of this instance's peer-server. This function exists
     * to support testing.
     *
     * @return  Address of the peer-server
     */
    virtual SockAddr getPeerSrvrAddr() const =0;

    /**
     * Blocks until at least one remote peer has established a connection via
     * the local peer-server.
     */
    virtual void waitForSrvrPeer() =0;

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
    virtual void notify(const DataSegId dataSegId) =0;

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

/// Interface for a subscriber's P2P manager
class SubP2pMgr : virtual public P2pMgr
                , public NoticeRcvr
                , public DataRcvr
{
public:
    using Pimpl = std::shared_ptr<SubP2pMgr>;

    /**
     * Creates a subscribing P2P manager.
     *
     * @param[in] subNode          Subscriber's node
     * @param[in] tracker          Socket addresses of potential peer-servers
     * @param[in] subPeerSrvrAddr  Socket address of subscriber's peer-server.
     *                             IP address *must not* specify all interfaces.
     *                             If port number is 0, then O/S will choose.
     * @param[in] maxPeers         Maximum number of peers. Might be adjusted
     *                             upwards.
     * @param[in] segSize          Size, in bytes, of canonical data-segment
     * @return                     Subscribing P2P manager
     * @see `getPeerSrvrAddr()`
     */
    static Pimpl create(
            SubNode&       subNode,
            Tracker        tracker,
            const SockAddr subPeerSrvrAddr,
            const unsigned maxPeers,
            const SegSize  segSize);

    /**
     * Returns the socket address of the subscribing P2P manager's peer-server.
     * This can be useful when the operating system chooses the port number.
     *
     * @return Socket address of peer-server
     * @see `create()`
     */
    virtual SockAddr getPeerSrvrAddr() const =0;

    /**
     * Receives a notice of potential peer servers from a remote peer.
     *
     * @param[in] notice       Potential peer servers
     * @param[in] peer         Associated local peer
     * @retval    `false`      Always
     */
    virtual bool recvNotice(const Tracker   notice,
                            Peer            peer) =0;
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
