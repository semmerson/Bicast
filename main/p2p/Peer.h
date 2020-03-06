/**
 * A local peer that communicates with its associated remote peer. Besides
 * sending notices to the remote peer, this class also creates and runs
 * independent threads that receive messages from a remote peer and pass them to
 * a peer message receiver.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
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

#include <memory>
#include <unordered_set>

namespace hycast {

class Peer; // Forward declaration

/**
 * Interface for an observer of a peer.
 */
class PeerObs
{
public:
    virtual ~PeerObs() noexcept
    {}

    /**
     * Indicates if product-information should be requested.
     *
     * @param[in] prodIndex  Identifier of product
     * @param[in] rmtAddr    Address of the remote peer
     * @retval    `true`     The product-information should be requested
     * @retval    `false`    The product-information should not be requested
     */
    virtual bool shouldRequest(
            ProdIndex       prodIndex,
            const SockAddr& rmtAddr) =0;

    /**
     * Indicates if a data-segment should be requested.
     *
     * @param[in] segId      Identifier of data-segment
     * @param[in] rmtAddr    Address of the remote peer
     * @retval    `true`     The data-segment should be requested
     * @retval    `false`    The data-segment should not be requested
     */
    virtual bool shouldRequest(
            const SegId&    segId,
            const SockAddr& rmtAddr) =0;

    /**
     * Returns product-information.
     *
     * @param[in] prodIndex  Identifier of product
     * @param[in] rmtAddr    Address of remote peer
     * @return               Product-information. Will test false if it doesn't
     *                       exist.
     */
    virtual ProdInfo  get(
            ProdIndex       prodIndex,
            const SockAddr& rmtAddr) =0;

    /**
     * Returns a data-segment.
     *
     * @param[in] segId      Identifier of data-segment
     * @param[in] rmtAddr    Address of remote peer
     * @return               Data-segment. Will test false if it doesn't exist.
     */
    virtual MemSeg  get(
            const SegId&    segId,
            const SockAddr& rmtAddr) =0;

    /**
     * Accepts product-information.
     *
     * @param[in] prodInfo  Product information
     * @param[in] rmtAddr   Socket address of remote peer
     * @retval    `true`    Product information was accepted
     * @retval    `false`   Product information was previously accepted
     */
    virtual bool hereIs(
            const ProdInfo& prodInfo,
            const SockAddr& rmtAddr) =0;

    /**
     * Accepts a data-segment.
     *
     * @param[in] tcpSeg   TCP-based data-segment
     * @param[in] rmtAddr  Socket address of remote peer
     * @retval    `true`   Chunk was accepted
     * @retval    `false`  Chunk was previously accepted
     */
    virtual bool hereIs(
            TcpSeg&         tcpSeg,
            const SockAddr& rmtAddr) =0;
};

class Peer final
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default construction.
     */
    Peer();

    /**
     * Server-side construction.
     *
     * @param[in]     sock      `::accept()`ed connection to the client peer
     * @param[in,out] portPool  Pool of port numbers for temporary servers
     * @param[in]     peerObs   Observer of this instance
     * @param[in]     isSource  Is this instance the source of data-products?
     */
    Peer(   TcpSock&  sock,
            PortPool& portPool,
            PeerObs&  peerObs,
            bool      isSource = false);

    /**
     * Client-side construction.
     *
     * @param[in] sock      `::connect()`ed connection to the server peer
     * @param[in] peerObs   Observer of this instance
     */
    Peer(   const SockAddr& rmtSrvrAddr,
            PeerObs&        peerObs);

    /**
     * Copy construction.
     *
     * @param[in] peer  Peer to be copied
     */
    Peer(const Peer& peer);

    ~Peer() noexcept;

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

    operator bool() const noexcept;

    Peer& operator=(const Peer& rhs);

    bool operator==(const Peer& rhs) const noexcept;

    bool operator<(const Peer& rhs) const noexcept;

    /**
     * Executes asynchronous tasks that call the member functions of the
     * constructor's `PeerMsgRcvr` argument. Doesn't return until the current
     * thread is canceled or a task throws an exception.
     *
     * @throws    std::system_error   System error
     * @throws    std::runtime_error  Remote peer closed the connection
     */
    void operator ()();

    /**
     * Halts execution. Terminates all subtasks. Causes `operator()()` to
     * return. If `terminate()` is called before this method, then this instance
     * will return immediately and won't execute. Idempotent.
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

    void request(const ProdIndex prodIndex) const;

    void request(const SegId& segId) const;

    size_t hash() const noexcept;

    std::string to_string() const noexcept;

#if 0
    /**
     * Handles a notice of available product-information.
     *
     * @param[in] prodIndex  Identifier of product
     */
    void acceptNotice(ProdIndex prodIndex);

    /**
     * Handles a notice of an available data-segment.
     *
     * @param[in] segId  Identifier of data-segment
     */
    void acceptNotice(const SegId& segId);

    /**
     * Handles a request for product-information.
     *
     * @param[in] prodIndex  Identifier of product
     */
    void acceptRequest(ProdIndex prodIndex);

    /**
     * Handles a request for a data-segment.
     *
     * @param[in] segId  Identifier of data-segment
     */
    void acceptRequest(const SegId& segId);

    /**
     * Accepts product-information from the remote peer.
     *
     * @param[in] prodInfo   Product-information
     * @retval    `true`     Accepted
     * @retval    `false`    Previously accepted
     */
    void accept(const ProdInfo& prodInfo);

    /**
     * Accepts a data-segment from the remote peer.
     *
     * @param[in] seg        Data-segment
     * @retval    `true`     Segment was accepted
     * @retval    `false`    Segment was previously accepted
     */
    void accept(TcpSeg& seg);
#endif
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
}

#endif /* MAIN_PEER_PEER_H_ */
