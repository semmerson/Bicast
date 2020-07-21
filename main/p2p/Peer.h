/**
 * A local peer that communicates with it's associated remote peer. Besides
 * sending notices to the remote peer, this class also creates and runs
 * independent threads that receive messages from the remote peer and passes
 * them to a peer manager.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Peer.h
 *  Created on: May 10, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PEER_PEER_H_
#define MAIN_PEER_PEER_H_

#include "hycast.h"
#include "PeerProto.h"
#include "SockAddr.h"

#include <memory>

namespace hycast {

/**
 * Interface for the manager of a peer that sends data to another peer.
 */
class SendPeerMgr
{
public:
    /**
     * Destroys.
     */
    virtual ~SendPeerMgr() noexcept =default;

    /**
     * Returns information on a product.
     *
     * @param[in] remote     Socket address of remote peer
     * @param[in] prodIndex  Index of product
     * @return               Information on product. Will test false if no such
     *                       information exists.
     * @threadsafety         Safe
     * @exceptionsafety      Strong guarantee
     * @cancellationpoint    No
     * @see `ProdInfo::operator bool()`
     */
    virtual ProdInfo getProdInfo(
            const SockAddr& remote,
            const ProdIndex prodIndex) =0;

    /**
     * Returns a data-segment
     *
     * @param[in] remote            Socket address of remote peer
     * @param[in] segId             Segment identifier
     * @return                      Data-segment. Will test false if no such
     *                              segment exists.
     * @throws    InvalidArgument   Segment identifier is invalid
     * @threadsafety                Safe
     * @exceptionsafety             Strong guarantee
     * @cancellationpoint           No
     * @see `MemSeg::operator bool()`
     */
    virtual MemSeg getMemSeg(
            const SockAddr& remote,
            const SegId&    segId) =0;
};

/**
 * Interface for the manager of a peer that exchanges data with a remote peer.
 */
class XcvrPeerMgr : public SendPeerMgr
{
public:
    virtual ~XcvrPeerMgr() noexcept =default;

    /**
     * Handles the remote node transitioning from not having a path to the
     * publisher of data-products to having one.
     *
     * @param[in] peer        Relevant peer
     * @throws    LogicError  Local node is publisher
     */
    virtual void pathToPub(Peer& peer) =0;

    /**
     * Handles the remote node transitioning from having a path to the publisher
     * of data-products to not having one.
     *
     * @param[in] peer        Relevant peer
     * @throws    LogicError  Local node is publisher
     */
    virtual void noPathToPub(Peer& peer) =0;

    /**
     * Indicates if product-information should be requested.
     *
     * @param[in] peer        Relevant peer
     * @param[in] prodIndex   Identifier of product
     * @retval    `true`      The product-information should be requested
     * @retval    `false`     The product-information should not be requested
     * @throws    LogicError  Local node is publisher
     */
    virtual bool shouldRequest(
            Peer&     peer,
            ProdIndex prodIndex) =0;

    /**
     * Indicates if a data-segment should be requested.
     *
     * @param[in] peer        Relevant peer
     * @param[in] segId       Identifier of data-segment
     * @retval    `true`      The data-segment should be requested
     * @retval    `false`     The data-segment should not be requested
     * @throws    LogicError  Local node is publisher
     */
    virtual bool shouldRequest(
            Peer&        peer,
            const SegId& segId) =0;

    /**
     * Accepts product-information.
     *
     * @param[in] peer        Relevant peer
     * @param[in] prodInfo    Product information
     * @retval    `true`      Product information was accepted
     * @retval    `false`     Product information was previously accepted
     * @throws    LogicError  Local node is publisher
     */
    virtual bool hereIs(
            Peer&           peer,
            const ProdInfo& prodInfo) =0;

    /**
     * Accepts a data-segment.
     *
     * @param[in] peer         Relevant peer
     * @param[in] tcpSeg      TCP-based data-segment
     * @retval    `true`      Chunk was accepted
     * @retval    `false`     Chunk was previously accepted
     * @throws    LogicError  Local node is publisher
     */
    virtual bool hereIs(
            Peer&   peer,
            TcpSeg& tcpSeg) =0;
};

/**
 * A peer of a peer-to-peer network.
 */
class Peer
{
public:
    class Impl;

protected:
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default construction.
     */
    Peer();

    /**
     * Constructs a publisher-peer.
     *
     * @param[in]     sock      `::accept()`ed connection to the client peer
     * @param[in,out] portPool  Pool of port numbers for temporary servers
     * @param[in]     peerMgr   Manager of publisher-peer
     */
    Peer(   TcpSock&     sock,
            PortPool&    portPool,
            SendPeerMgr& peerMgr);

    /**
     * Constructs a server-side subscriber-peer.
     *
     * @param[in]     sock           `::accept()`ed connection to the client peer
     * @param[in,out] portPool       Pool of port numbers for temporary servers
     * @param[in]     lclNodeType    Type of local node
     * @param[in]     peerMgr        Manager of subscriber-peer
     */
    Peer(   TcpSock&     sock,
            PortPool&    portPool,
            NodeType     lclNodeType,
            XcvrPeerMgr& subPeerMgrApi);

    /**
     * Constructs a client-side subscriber-peer.
     *
     * @param[in] rmtSrvrAddr    Address of remote peer-server
     * @param[in] lclNodeType    Type of local node
     * @param[in] subPeerMgrApi  Manager of subscriber peer
     * @throws    LogicError     `lclNodeType == NodeType::PUBLISHER`
     */
    Peer(   const SockAddr& rmtSrvrAddr,
            const NodeType  lclNodeType,
            XcvrPeerMgr&    peerMgr);

    /**
     * Copy construction.
     *
     * @param[in] peer  Peer to be copied
     */
    Peer(const Peer& peer);

    operator bool() const noexcept;

    /**
     * Returns the socket address of the remote peer. On the client-side, this
     * will be the address of the peer-server; on the server-side, this will be
     * the address of the `accept()`ed socket.
     *
     * @return Socket address of the remote peer.
     */
    const SockAddr getRmtAddr() const noexcept;

    /**
     * Returns the local socket address.
     *
     * @return Local socket address
     */
    const SockAddr getLclAddr() const noexcept;

    Peer& operator=(const Peer& rhs);

    bool operator==(const Peer& rhs) const noexcept;

    bool operator<(const Peer& rhs) const noexcept;

    /**
     * Executes asynchronous tasks that call the member functions of the
     * constructor's peer manager. Doesn't return until `halt()` is called a
     * task throws an exception. If `halt()` is called before this method, then
     * this instance will return immediately and won't execute. Idempotent.
     *
     * @throws    std::system_error   System error
     * @throws    std::runtime_error  Remote peer closed the connection
     */
    void operator ()() const;

    /**
     * Halts execution. Causes `operator()()` to return if it has been called.
     *
     * @cancellationpoint No
     */
    void halt() const noexcept;

    /**
     * Notifies the remote peer about the availability of product-information.
     *
     * @param[in] prodIndex  Identifier of the product
     */
    void notify(ProdIndex prodIndex) const;

    /**
     * Notifies the remote peer about the availability of a data-segment.
     *
     * @param[in] segId  Identifier of data-segment
     */
    void notify(const SegId& segId) const;

    /**
     * Returns the hash value of this instance.
     *
     * @return Hash value of this instance
     */
    size_t hash() const noexcept;

    /**
     * Returns a string representation of this instance.
     *
     * @return String representation of this instance
     */
    std::string to_string() const noexcept;

    /**
     * Indicates if this instance resulted from a call to `::connect()`.
     *
     * @retval `false`  No
     * @retval `true`   Yes
     */
    bool isFromConnect() const noexcept;

    /**
     * Indicates if the remote node is a path to the publisher of data-products.
     *
     * @retval `false`     Remote node is not path to source
     * @retval `true`      Remote node is path to source
     * @throws LogicError  This instance is a publisher-peer
     */
    bool isPathToPub() const noexcept;

    /**
     * Notifies the remote peer that this local node just transitioned to being
     * a path to the source of data-products.
     *
     * @throws LogicError  This instance is a publisher-peer
     */
    void gotPath() const;

    /**
     * Notifies the remote peer that this local node just transitioned to not
     * being a path to the source of data-products.
     *
     * @throws LogicError  This instance is a publisher-peer
     */
    void lostPath() const;

    /**
     * Requests information on a product from the remote peer.
     *
     * @param[in] prodIndex   Product index
     * @throws    LogicError  This instance is a publisher-peer
     */
    void request(const ProdIndex prodIndex) const;

    /**
     * Requests a data-segment from the remote peer.
     *
     * @param[in] segId       Data-segment identifier
     * @throws    LogicError  This instance is a publisher-peer
     */
    void request(const SegId& segId) const;
};

} // namespace

namespace std {

template<>
struct hash<hycast::Peer>
{
    size_t operator()(const hycast::Peer& peer) const noexcept
    {
        return peer.hash();
    }
};

template<>
struct equal_to<hycast::Peer>
{
    size_t operator()(
            const hycast::Peer& peer1,
            const hycast::Peer& peer2) const noexcept
    {
        return peer1 == peer2;
    }
};

} // "std" namespace

#endif /* MAIN_PEER_PEER_H_ */
