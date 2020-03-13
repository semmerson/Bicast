/**
 * Peer-to-peer protocol.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PeerProto.h
 *  Created on: Nov 4, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PROTOCOL_PEERPROTO_H_
#define MAIN_PROTOCOL_PEERPROTO_H_

#include "hycast.h"
#include "NodeType.h"
#include "PortPool.h"
#include "Socket.h"

#include <memory>

namespace hycast {

/**
 * Interface for an observer of a `PeerProto`.
 */
class PeerProtoObs
{
public:
    virtual ~PeerProtoObs() noexcept
    {}

    /**
     * Handles the remote node transitioning from not having a path to the
     * source of data-products to having one.
     */
    virtual void pathToSrc() noexcept =0;

    /**
     * Handles the remote node transitioning from having a path to the source of
     * data-products to not having one.
     */
    virtual void noPathToSrc() noexcept =0;

    /**
     * Handles a notice of available product-information.
     *
     * @param[in] prodIndex  Index of product
     */
    virtual void acceptNotice(ProdIndex prodIndex) =0;

    /**
     * Handles a notice of an available data-segment.
     *
     * @param[in] segId  Data-segment identifier
     */
    virtual void acceptNotice(const SegId& segId) =0;

    /**
     * Handles a request for product-information.
     *
     * @param[in] prodIndex  Identifier of product
     */
    virtual void acceptRequest(ProdIndex prodIndex) =0;

    /**
     * Handles a request for a data-segment.
     *
     * @param[in] segId  Identifier of data-segment
     */
    virtual void acceptRequest(const SegId& segId) =0;

    /**
     * Accepts product-information from the remote peer.
     *
     * @param[in] prodInfo   Product-information
     * @retval    `true`     Accepted
     * @retval    `false`    Previously accepted
     */
    virtual void accept(const ProdInfo& prodInfo) =0;

    /**
     * Accepts a data-segment from the remote peer.
     *
     * @param[in] seg        Data-segment
     * @retval    `true`     Segment was accepted
     * @retval    `false`    Segment was previously accepted
     */
    virtual void accept(TcpSeg& seg) =0;
};

class PeerProto
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    PeerProto(Impl* impl);

public:
    PeerProto() =default;

    /**
     * Server-side construction.
     *
     * @param[in]     sock      Server's listening TCP socket
     * @param[in,out] portPool  Pool of port numbers for transient servers
     * @param[in]     observer  Observer of this instance
     * @param[in]     nodeType  Type of node
     * @cancellationpoint       Yes
     */
    PeerProto(
            TcpSock&       sock,
            PortPool&      portPool,
            NodeType&      nodeType,
            PeerProtoObs&  observer);

    /**
     * Client-side construction.
     *
     * @param[in] rmtSrvrAddr  Socket address of remote peer-server
     * @param[in] nodeType     Type of local node
     * @param[in] observer     Observer of this instance
     * @cancellationpoint      Yes
     */
    PeerProto(
            const SockAddr& rmtSrvrAddr,
            NodeType&       nodeType,
            PeerProtoObs&   observer);

    operator bool() const;

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
     * Indicates if, after construction, the remote node is a path to the source
     * of data-products. The result can be changed later by this instance
     * calling `pathToSrc()` and `noPathToSrc()` after `operator()` is called.
     *
     * @retval `false`  Remove node is not a path to source
     * @retval `true`   Remove node is a path to source
     */
    bool isPathToSrc() const noexcept;

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
     * Notifies the remote peer that this local node just transitioned to being
     * a path to the source of data-products.
     */
    void gotPath() const;

    /**
     * Notifies the remote peer that this local node just transitioned to not
     * being a path to the source of data-products.
     */
    void lostPath() const;

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
     * Requests information on a product from the remote peer.
     *
     * @param[in] prodId     Product identifier
     * @cancellationpoint    Yes
     */
    void request(ProdIndex prodId) const;

    /**
     * Requests a data-segment from the remote peer.
     *
     * @param[in] segId      Segment identifier
     * @cancellationpoint    Yes
     */
    void request(SegId segId) const;

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
};

} // namespace

#endif /* MAIN_PROTOCOL_PEERPROTO_H_ */
