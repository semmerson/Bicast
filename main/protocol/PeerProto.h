/**
 * Peer-to-peer protocol.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PeerProto.h
 *  Created on: Nov 4, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PROTOCOL_PEERPROTO_H_
#define MAIN_PROTOCOL_PEERPROTO_H_

#include <main/inet/PortPool.h>
#include <main/inet/Socket.h>
#include "hycast.h"
#include "NodeType.h"
#include <memory>

namespace hycast {

/**
 * Interface for a peer that sends data to another peer (publisher &
 * subscriber).
 */
class SendPeer
{
public:
    virtual ~SendPeer() noexcept =default;

    /**
     * Handles a request for product-information from the remote peer. Won't be
     * called if the remote node is the publisher.
     *
     * @param[in] prodIndex  Identifier of product
     */
    virtual void sendMe(const ProdIndex prodIndex) =0;

    /**
     * Handles a request for a data-segment from the remote peer. Won't be
     * called if the remote node is the publisher.
     *
     * @param[in] segId  Identifier of data-segment
     */
    virtual void sendMe(const SegId& segId) =0;
};

/**
 * Interface for a peer that receives data from another peer (subscriber only).
 */
class RecvPeer
{
public:
    virtual ~RecvPeer() noexcept =default;

    /**
     * Returns a view of this instance as a `SendPeer` (every `RecvPeer`s should
     * also be a `SendPeer`). This avoids the multiple inheritance diamond in
     * which `SendPeer` is the base, `PubPeer::Impl` and `RecvPeer` both derive
     * from it, and `SubPeer::Impl` derives from both of them, which would
     * require virtual inheritance with its complications and performance hits.
     *
     * @return View of this instance as a `SendPeer`
     */
    virtual SendPeer& asSendPeer() noexcept =0;

    /**
     * Handles the remote node transitioning from not having a path to the
     * publisher of data-products to having one. Won't be called if the remote
     * node is the publisher.
     */
    virtual void pathToPub() =0;

    /**
     * Handles the remote node transitioning from having a path to the publisher
     * of data-products to not having one. Won't be called if he remote node is
     * the publisher.
     */
    virtual void noPathToPub() =0;

    /**
     * Handles a notice of available product-information from the remote peer.
     *
     * @param[in] prodIndex  Index of product
     */
    virtual void available(ProdIndex prodIndex) =0;

    /**
     * Handles a notice of an available data-segment from the remote peer.
     *
     * @param[in] segId  Data-segment identifier
     */
    virtual void available(const SegId& segId) =0;

    /**
     * Accepts product-information from the remote peer.
     *
     * @param[in] prodInfo   Product-information
     * @retval    `true`     Accepted
     * @retval    `false`    Previously accepted
     */
    virtual void hereIs(const ProdInfo& prodInfo) =0;

    /**
     * Accepts a data-segment from the remote peer.
     *
     * @param[in] seg        Data-segment
     * @retval    `true`     Segment was accepted
     * @retval    `false`    Segment was previously accepted
     */
    virtual void hereIs(TcpSeg& seg) =0;
};

/**
 * Peer protocol
 */
class PeerProto final
{
public:
    class Impl;

protected:
    std::shared_ptr<Impl> pImpl;

    PeerProto(Impl* impl);

public:
    PeerProto() =default;

    /**
     * Publisher & subscriber sending-side construction.
     *
     * @param[in]     sock         Server's listening TCP socket
     * @param[in,out] portPool     Pool of port numbers for transient servers
     * @param[in]     peer         Sending Peer
     * @cancellationpoint          Yes
     */
    PeerProto(
            TcpSock&  sock,
            PortPool& portPool,
            SendPeer& peer);

    /**
     * Subscriber server-side construction.
     *
     * @param[in]     sock         Server's listening TCP socket
     * @param[in,out] portPool     Pool of port numbers for transient servers
     * @param[in]     lclNodeType  Type of local node
     * @param[in]     subPeer      Subscriber peer
     * @cancellationpoint          Yes
     */
    PeerProto(
            TcpSock&   sock,
            PortPool&  portPool,
            NodeType   lclNodeType,
            RecvPeer&  subPeer);

    /**
     * Subscriber client-side construction.
     *
     * @param[in] rmtSrvrAddr  Socket address of remote peer-server
     * @param[in] lclNodeType  Type of local node
     * @throws    LogicError   `lclNodeType == NodeType::PUBLISHER`
     * @param[in] subPeer      Subscriber peer
     * @cancellationpoint      Yes
     */
    PeerProto(
            const SockAddr& rmtSrvrAddr,
            const NodeType  lclNodeType,
            RecvPeer&       subPeer);

    /**
     * Indicates if this instance is valid (i.e., not default constructed).
     *
     * @retval `true`   Valid
     * @retval `false`  Not valid
     */
    operator bool() const;

    /**
     * Returns the type of the remote node.
     *
     * @return  Type of remote node
     */
    NodeType getRmtNodeType() const noexcept;

    /**
     * Returns the socket address of the remote peer.
     *
     * @return             Socket address of remote peer
     * @cancellationpoint  No
     */
    SockAddr getRmtAddr() const;

    /**
     * Returns the socket address of the local peer.
     *
     * @return             Socket address of local peer
     * @cancellationpoint  No
     */
    SockAddr getLclAddr() const;

    /**
     * Returns a string representation of this instance.
     *
     * @return             String representation of this instance
     * @cancellationpoint  No
     */
    std::string to_string() const;

    /**
     * Executes this instance.
     *
     * @throws SystemError   System error
     * @throws RuntimeError  Remote peer closed the connection
     * @throws LogicError    This method has already been called
     */
    void operator()() const;

    /**
     * Halts execution of this instance by shutting-down the connection with the
     * remote peer.
     */
    void halt() const;

    /**
     * Notifies the remote peer of available product information.
     *
     * @param[in] prodId     Product identifier
     * @cancellationpoint    Yes
     */
    void notify(ProdIndex prodId) const;

    /**
     * Notifies the remote peer of an available data-segment.
     *
     * @param[in] id       Data-segment ID
     * @cancellationpoint  Yes
     */
    void notify(const SegId& id) const;

    /**
     * Sends product-information to the remote peer.
     *
     * @param[in] info     Product-information
     * @cancellationpoint  Yes
     */
    void send(const ProdInfo& info) const;

    /**
     * Sends a data-segment to the remote peer.
     *
     * @param[in] seg      Data-segment
     * @cancellationpoint  Yes
     */
    void send(const MemSeg& seg) const;

    /**
     * Notifies the remote peer that this local node just transitioned to being
     * a path to the publisher of data-products.
     *
     * @throws LogicError    This instance is a publisher
     * @cancellationpoint    Yes
     */
    void gotPath() const;

    /**
     * Notifies the remote peer that this local node just transitioned to not
     * being a path to the publisher of data-products.
     *
     * @throws LogicError    This instance is a publisher
     * @cancellationpoint    Yes
     */
    void lostPath() const;

    /**
     * Requests information on a product from the remote peer.
     *
     * @param[in] prodIndex   Product index
     * @throws    LogicError  This instance is a publisher
     * @cancellationpoint     Yes
     */
    void request(ProdIndex prodIndex) const;

    /**
     * Requests a data-segment from the remote peer.
     *
     * @param[in] segId       Segment identifier
     * @throws    LogicError  This instance is a publisher
     * @cancellationpoint     Yes
     */
    void request(SegId segId) const;
};

} // namespace

#endif /* MAIN_PROTOCOL_PEERPROTO_H_ */
