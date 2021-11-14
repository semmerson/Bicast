/**
 * This file defines the Peer class. The Peer class handles low-level,
 * bidirectional messaging with its remote counterpart.
 *
 *  @file:  Peer.cpp
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
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
#include "config.h"

#include "HycastProto.h"
#include "logging.h"
#include "Peer.h"
#include "ThreadException.h"

#include <atomic>
#include <cassert>
#include <functional>
#include <list>
#include <sstream>
#include <queue>
#include <thread>
#include <unordered_map>
#include <utility>

namespace hycast {

/**
 * Abstract base implementation of the `Peer` class.
 */
class Peer::Impl
{
    /**
     * Puts sockets in increasing order according to the local port number.
     *
     * @param[in,out] socks  Sockets to be put in order
     */
    void orderClntSocks(TcpSock socks[3]) const noexcept {
        if (socks[1].getLclPort() < socks[0].getLclPort())
            socks[1].swap(socks[0]);

        if (socks[2].getLclPort() < socks[1].getLclPort()) {
            socks[2].swap(socks[1]);
            if (socks[1].getLclPort() < socks[0].getLclPort())
                socks[1].swap(socks[0]);
        }
    }

    /**
     * Puts sockets in increasing order according to the remote port number.
     *
     * @param[in,out] socks  Sockets to be put in order
     */
    void orderSrvrSocks(TcpSock socks[3]) const noexcept {
        if (socks[1].getRmtPort() < socks[0].getRmtPort())
            socks[1].swap(socks[0]);

        if (socks[2].getRmtPort() < socks[1].getRmtPort()) {
            socks[2].swap(socks[1]);
            if (socks[1].getRmtPort() < socks[0].getRmtPort())
                socks[1].swap(socks[0]);
        }
    }

protected:
    mutable Mutex      sockMutex;
    mutable Mutex      exceptMutex;
    P2pNode&           node;
    const bool         clientSide;  ///< Instance was constructed as a client?
    /*
     * If a single socket is used for asynchronous communication and reading and
     * writing occur on the same thread, then deadlock will occur if both
     * receive buffers are full and each end is trying to write. To prevent
     * this, three sockets are used and a thread that reads from one socket will
     * write to another. The pipeline might block for a while as messages are
     * processed, but it won't deadlock.
     */
    Xprt               noticeXprt;
    Xprt               requestXprt;
    Xprt               dataXprt;
    Thread             noticeReader;
    Thread             requestReader;
    Thread             dataReader;
    Thread             requestWriter;
    SockAddr           rmtSockAddr;
    // States of this instance:
    enum class State {
        INITED,
        STARTING,
        STARTED,
        STOPPING
    };
    std::atomic<State> state;       /// State of this instance
    std::exception_ptr exPtr;       /// Internal thread exception
    std::atomic<bool>  connected;   /// Connected to remote peer?

    /**
     * Assigns sockets to this instance's notice, request, and data transports.
     *
     * @param[in] socks  Sockets to be assigned
     */
    void assignSocks(TcpSock socks[3]) {
        noticeXprt  = Xprt{socks[0]};
        requestXprt = Xprt{socks[1]};
        dataXprt    = Xprt{socks[2]};
    }

    /**
     * Assigns server-side-created sockets to this instance's notice, request,
     * and data sockets
     *
     * @param[in,out] socks  Sockets to be assigned
     */
    void assignSrvrSocks(TcpSock socks[3]) {
        orderSrvrSocks(socks);
        assignSocks(socks);
    }

    /**
     * Assigns client-side created sockets to this instance's notice, request,
     * and data sockets
     *
     * @param[in,out] socks  Sockets to be assigned
     */
    void assignClntSocks(TcpSock socks[3]) {
        orderClntSocks(socks);
        assignSocks(socks);
    }

    /**
     * Vets the protocol version used by the remote peer.
     *
     * @param[in] protoVers  Remote protocol version
     * @param[in] rmtAddr    IP address of remote peer
     * @throw LogicError     Remote peer uses unsupported protocol
     */
    void vetProtoVers(decltype(PROTOCOL_VERSION) protoVers,
                      const InetAddr             rmtAddr) {
        if (protoVers != PROTOCOL_VERSION)
            throw LOGIC_ERROR("Remote peer " +
                    rmtAddr.to_string() + " uses protocol " +
                    std::to_string(protoVers) + "; not " +
                    std::to_string(PROTOCOL_VERSION));
    }

    /**
     * Sets the internal thread exception.
     */
    void setExPtr() {
        Guard guard{exceptMutex};
        if (!exPtr)
            exPtr = std::current_exception();
    }

    /**
     * Throws an exception if the state of this instance isn't appropriate or an
     * exception has been thrown by one of this instance's internal threads.
     *
     * @throw LogicError  Instance isn't in appropriate state
     */
    void throwIf() {
        if (state != State::STARTED)
            throw LOGIC_ERROR("Instance isn't in state STARTED");

        bool throwEx = false;
        {
            Guard guard{exceptMutex};
            throwEx = static_cast<bool>(exPtr);
        }
        if (throwEx)
            std::rethrow_exception(exPtr);
    }

    /**
     * Exchanges protocol version with remote peer.
     *
     * @retval    `true`      Success
     * @retval    `false`     Connection lost
     * @throw     LogicError  Remote peer uses unsupported protocol
     */
    bool xchgProtoVers() {
        auto protoVers = PROTOCOL_VERSION;
        bool success = clientSide
            ? noticeXprt.read(protoVers) && noticeXprt.write(PROTOCOL_VERSION)
            : noticeXprt.write(PROTOCOL_VERSION) && noticeXprt.read(protoVers);

        if (success)
            vetProtoVers(protoVers, noticeXprt.getRmtAddr().getInetAddr());

        return success;
    }

    /**
     * Sends data to the remote peer.
     *
     * @param[in] pduId    Protocol Data Unit identifier
     * @param[in] data     Data
     * @retval    `false`  No connection. Connection was lost or `start()`
     *                     wasn't called.
     * @retval    `true`   Success
     */
    bool send(PduId pduId, const XprtAble& data) {
        if (!connected)
            return false;

        return connected = dataXprt.send(pduId, data);
    }
    inline bool send(const ProdInfo& data) {
        LOG_DEBUG("Sending product information to %s",
                dataXprt.to_string().data());
        return send(PduId::PROD_INFO, data);
    }
    inline bool send(const DataSeg& data) {
        LOG_DEBUG("Sending data segment to %s", dataXprt.to_string().data());
        return send(PduId::DATA_SEG, data);
    }

    /**
     * Receives a request for data from the remote peer. Sends the data to the
     * remote peer if it exists.
     *
     * @tparam    ID       Identifier of requested data
     * @tparam    DATA     Type of requested data
     * @param[in] xprt     Transport
     * @param[in] peer     Associated local peer
     * @retval    `true`   Data doesn't exist or was successfully sent
     * @retval    `false`  Connection lost
     */
    template<class ID, class DATA>
    bool rcvRequest(Xprt& xprt, Peer peer) {
        bool success = false;
        ID   id;
        LOG_DEBUG("Receiving request");
        if (id.read(xprt)) {
            auto data = node.recvRequest(id, peer);
            success = !data || send(data); // Data doesn't exist or was sent
            if (success) LOG_DEBUG("Data sent");
        }
        return success;
    }

    /**
     * Dispatch function for processing requests from the remote peer.
     *
     * @param[in] pduId         PDU ID
     * @param[in] peer          Associated local peer
     * @retval    `false`       Connection lost
     * @retval    `true`        Success
     * @throw     LogicError    Message is unknown or unsupported
     */
    bool processRequest(Xprt::PduId pduId, Peer peer) {
        bool success = false;

        if (PduId::PROD_INFO_REQUEST == pduId) {
            LOG_NOTE("Reading product-information request from %s",
                    requestXprt.to_string().data());
            success = rcvRequest<ProdIndex, ProdInfo>(requestXprt, peer);
        }
        else if (PduId::DATA_SEG_REQUEST == pduId) {
            LOG_NOTE("Reading data-segment request from %s",
                    requestXprt.to_string().data());
            success = rcvRequest<DataSegId, DataSeg>(requestXprt, peer);
        }
        else {
            throw LOGIC_ERROR("Invalid PDU type: " + std::to_string(pduId));
        }

        return success;
    }

    /**
     * Reads and processes messages from the remote peer. Doesn't return until
     * either EOF is encountered or an error occurs.
     *
     * @param[in] xprt      Transport
     * @param[in] dispatch  Dispatch function for processing incoming PDU-s
     * @throw LogicError    Message type is unknown or unsupported
     */
    void runReader(Xprt xprt, Xprt::Dispatch dispatch) {
        try {
            while (connected) {
                connected = xprt.recv(dispatch);
            }
            LOG_NOTE("Connection closed");
        }
        catch (const std::exception& ex) {
            log_error(ex);
            setExPtr();
            connected = false;
        }
    }

    /**
     * Starts the internal threads needed to read and service messages from the
     * remote peer.
     *
     * @param[in] peer  Associated local peer
     */
    virtual void startThreads(Peer peer) =0;

    /**
     * Stops and joins the threads that are reading and servicing messages from
     * the remote peer.
     */
    virtual void stopAndJoinThreads() =0; // Idempotent

public:
    /**
     * Constructs.
     *
     * @param[in] node        P2P node
     * @param[in] clientSide  Instance is a client?
     */
    Impl(P2pNode& node, bool clientSide)
        : sockMutex()
        , exceptMutex()
        , node(node)
        , clientSide(clientSide)
        , noticeXprt()
        , requestXprt()
        , dataXprt()
        , noticeReader()
        , requestReader()
        , dataReader()
        , requestWriter()
        , rmtSockAddr()
        , state(State::INITED)
        , exPtr()
        , connected{false}
    {}

    Impl(const Impl& impl) =delete; // Rule of three

    virtual ~Impl() noexcept {
        try {
            stop(); // Idempotent
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }
    }

    Impl& operator=(const Impl& rhs) noexcept =delete; // Rule of three

    bool isClient() const noexcept {
        return clientSide;
    }

    /**
     * Starts this instance. Does the following:
     *   - If client-side constructed, blocks while connecting to the remote
     *     peer
     *   - Creates threads on which
     *       - The sockets are read; and
     *       - The P2P node is called.
     *
     * @param[in] peer     Associated local peer
     * @retval    `false`  Peer is client-side and couldn't connect with remote
     *                     peer
     * @retval    `false`  `stop()` was called
     * @retval    `true`   Success
     * @throw LogicError   Already called
     * @throw LogicError   Remote peer uses unsupported protocol
     * @throw SystemError  Thread couldn't be created
     * @see   `stop()`
     */
    bool start(Peer peer) {
        bool  success = false;  // Failure default

        State expected{State::INITED};
        if (!state.compare_exchange_strong(expected, State::STARTING)) {
            if (expected == State::STARTING || expected == State::STARTED)
                throw LOGIC_ERROR("start() already called");
        }
        else {
            startThreads(peer); // Depends on `rmtNodeType`

            auto expected = State::STARTING;
            if (state.compare_exchange_strong(
                    expected, State::STARTED)) {
                success = true;
                // `stop()` is now effective
            }
            else {
                stopAndJoinThreads();
            }
        } // Instance is initialized

        return success;
    }

    /**
     * Returns the socket address of the remote peer.
     *
     * @return Socket address of remote peer
     */
    SockAddr getRmtAddr() const noexcept {
        return rmtSockAddr;
    }

    /**
     * Stops this instance from serving its remote counterpart. Causes the
     * threads serving the remote peer to terminate. If called before `start()`,
     * then the remote peer will not have been served.
     *
     * Idempotent.
     *
     * @see   `start()`
     */
    void stop() {
        auto expected = State::INITED;
        if (!state.compare_exchange_strong(expected, State::STOPPING)) {

            expected = State::STARTING;
            if (!state.compare_exchange_strong(expected, State::STOPPING)) {

                expected = State::STARTED;
                if (state.compare_exchange_strong(expected, State::STOPPING)) {
                    stopAndJoinThreads();
                }
            }
        }
    }

    /**
     * Returns a string representation of this instance.
     *
     * @return  String representation of this instance
     */
    String to_string() const {
        return noticeXprt.to_string();
    }

    bool notify(const PubPath notice) {
        throwIf();
        return noticeXprt.send(PduId::PUB_PATH_NOTICE, notice);
    }
    bool notify(const PeerSrvrAddrs peerSrvrAddrs) {
        throwIf();
        return dataXprt.send(PduId::PEER_SRVR_ADDRS, peerSrvrAddrs);
    }
    bool notify(const ProdIndex notice) {
        throwIf();
        LOG_NOTE("Sending product-information notice to %s",
                noticeXprt.to_string().data());
        return noticeXprt.send(PduId::PROD_INFO_NOTICE, notice);
    }
    bool notify(const DataSegId& notice) {
        throwIf();
        LOG_NOTE("Sending data-segment notice to %s",
                noticeXprt.to_string().data());
        return noticeXprt.send(PduId::DATA_SEG_NOTICE, notice);
    }

    /**
     * Indicates if the remote peer has a path to the publisher.
     *
     * @retval `true`   Remote peer has a path to the publisher
     * @retval `false`  Remote peer does not have a path to the publisher
     */
    virtual bool rmtIsPubPath() const noexcept =0;
};

Peer::Peer(Impl* impl)
    : pImpl(impl)
{}

Peer::Peer(SharedPtr& pImpl)
    : pImpl(pImpl)
{}

bool Peer::isClient() const noexcept {
    return pImpl->isClient();
}

bool Peer::start() {
    return pImpl->start(*this);
}

SockAddr Peer::getRmtAddr() noexcept {
    return pImpl->getRmtAddr();
}

void Peer::stop() {
    pImpl->stop();
}

Peer::operator bool() const {
    return static_cast<bool>(pImpl);
}

size_t Peer::hash() const noexcept {
    /*
     * The underlying pointer is used instead of `pImpl->hash()` because
     * hashing a client-side socket in the implementation won't work until
     * `connect()` returns -- which could be a while -- and `PeerSet`, at least,
     * requires a hash before it calls `Peer::start()`.
     *
     * This means, however, that it's possible to have multiple client-side
     * peers connected to the same remote host within the same process.
     */
    return std::hash<Impl*>()(pImpl.get());
}

bool Peer::operator<(const Peer& rhs) const noexcept {
    // Must be consistent with `hash()`
    return pImpl.get() < rhs.pImpl.get();
}

bool Peer::operator==(const Peer& rhs) const noexcept {
    return !(*this < rhs) && !(rhs < *this);
}

bool Peer::operator!=(const Peer& rhs) const noexcept {
    return (*this < rhs) || (rhs < *this);
}

String Peer::to_string() const {
    return pImpl ? pImpl->to_string() : "<unset>";
}

bool Peer::notify(const PubPath notice) const {
    return pImpl->notify(notice);
}

bool Peer::notify(const PeerSrvrAddrs notice) const {
    return pImpl->notify(notice);
}

bool Peer::notify(const ProdIndex notice) const {
    return pImpl->notify(notice);
}

bool Peer::notify(const DataSegId& notice) const {
    return pImpl->notify(notice);
}

bool Peer::rmtIsPubPath() const noexcept {
    return pImpl->rmtIsPubPath();
}

/******************************************************************************/

/**
 * Publisher's peer implementation. The peer will be server-side constructed.
 */
class PubPeerImpl final : public Peer::Impl
{
    P2pNode& node;

protected:
    void startThreads(Peer peer) override {
        requestReader = Thread(&PubPeerImpl::runReader, this, requestXprt,
                [&](Xprt::PduId pduId, Xprt xprt) {
                        return processRequest(pduId, peer);});

        if (log_enabled(LogLevel::DEBUG)) {
            std::ostringstream threadId;
            threadId << requestReader.get_id();
            LOG_DEBUG("Request reader thread is %s", threadId.str().data());
        }
    }

    /**
     * Idempotent.
     */
    void stopAndJoinThreads() override {
        state = State::STOPPING;

        if (requestXprt) {
            LOG_DEBUG("Shutting down request transport");
            requestXprt.shutdown();
        }
        if (requestReader.joinable())
            requestReader.join();
    }

public:
    /**
     * Constructs server-side.
     *
     * @param[in] node          Publisher's P2P node
     * @param[in] socks         Sockets connected to the remote peer
     * @throw     RuntimeError  Lost connection
     */
    PubPeerImpl(P2pNode& node, TcpSock socks[3])
        : Impl(node, false)
        , node(node)
    {
        assignSrvrSocks(socks);
        rmtSockAddr = noticeXprt.getRmtAddr();
        if (!xchgProtoVers())
            throw RUNTIME_ERROR("Lost connection with " +
                    rmtSockAddr.to_string());
        connected = true;
    }

    bool rmtIsPubPath() const noexcept override {
        return true; // Because connected to me
    }
};

PubPeer::PubPeer(P2pNode& node, TcpSock socks[3])
    : Peer(new PubPeerImpl(node, socks))
{}

/******************************************************************************/

/**
 * Subscriber's peer implementation. May be server-side or client-side
 * constructed.
 */
class SubPeerImpl final : public Peer::Impl
{
    /**
     * Class for requests sent to a remote peer. Implemented as a discriminated
     * union so that it has a known size and, consequently, supports being in
     * a container.
     */
    class Request : public NoteReq {
    public:
        /**
         * Default constructs. The instance will test false.
         *
         * @see `operator bool()`
         */
        Request()
            : NoteReq()
        {}

        /**
         * Constructs a request for product-information.
         *
         * @param[in] prodIndex  Index of product
         */
        Request(const ProdIndex prodIndex)
            : NoteReq(prodIndex)
        {}

        /**
         * Constructs a request for a data-segment.
         *
         * @param[in] dataSegId  Identifier of the data-segment
         */
        Request(const DataSegId dataSegId)
            : NoteReq(dataSegId)
        {}

        ~Request() noexcept {
        }

        /**
         * Tells the P2P node about a request that wasn't satisfied by the
         * remote peer.
         *
         * @param[in] node  P2P node
         * @param[in] peer  Associated local peer
         * @see `SubP2pNode::missed()`
         */
        void missed(SubP2pNode& node, Peer peer) const {
            if (id == Id::PROD_INDEX) {
                node.missed(prodIndex, peer);
            }
            else if (id == Id::DATA_SEG_ID) {
                node.missed(dataSegId, peer);
            }
            else {
                throw LOGIC_ERROR("Invalid request ID: " +
                        std::to_string((int)id));
            }
        }

        /**
         * Indicates if this instance requested a given product information.
         *
         * @param[in] prodInfo  Product information
         * @retval    `true`    This instance requested it
         * @retval    `false`   This instance did not request it
         */
        inline bool requested(const ProdInfo& prodInfo) const noexcept {
            return id == Id::PROD_INDEX && prodIndex == prodInfo.getIndex();
        }

        /**
         * Indicates if a data-segment matches this instance.
         *
         * @param[in] dataSeg   Data-segment
         * @retval    `true`    This instance requested it
         * @retval    `false`   This instance did not request it
         */
        inline bool requested(const DataSeg& dataSeg) const noexcept {
            return id == Id::DATA_SEG_ID && dataSegId == dataSeg.getId();
        }
    };

    /**
     * A FIFO queue of requests.
     */
    class RequestQ {
        mutable Mutex       mutex;
        mutable Cond        cond;
        std::queue<Request> queue;
        bool                stopped;

    public:
        /**
         * Default constructs.
         */
        RequestQ()
            : mutex()
            , cond()
            , queue()
            , stopped(false)
        {}

        RequestQ(const RequestQ& queue) =delete;
        RequestQ operator=(const RequestQ& queue) =delete;
        ~RequestQ() =default;

        /**
         * Adds a request to the tail of the queue.
         *
         * @param[in] request  Request to be added
         * @return             Number of requests in the queue
         */
        size_t push(const Request& request) {
            Guard guard{mutex};
            queue.push(request);
            cond.notify_all();
            return queue.size();
        }

        /**
         * Indicates if the queue is empty.
         *
         * @retval `true`   Queue is empty
         * @retval `false`  Queue is not empty
         */
        bool empty() const {
            Guard guard{mutex};
            return queue.empty();
        }

        /**
         * Returns the request at the head of the queue.
         *
         * @return  Oldest request
         */
        Request& front() {
            Guard guard{mutex};
            return queue.front();
        }

        /**
         * Deletes the request at the head of the queue.
         *
         * @throw OutOfRange  Queue is empty
         */
        void pop() {
            Guard guard{mutex};
            if (queue.empty())
                throw OUT_OF_RANGE("Queue is empty");
            queue.pop();
        }

        /**
         * Removes and returns the request at the head of the queue. Blocks
         * until it exists or `stop()` is called.
         *
         * @return  Head request or false one if `stop()` was called
         * @see     `stop()`
         */
        Request waitGet() {
            Lock lock{mutex};
            while (queue.empty() && !stopped)
                cond.wait(lock);
            if (stopped)
                return Request{};
            const auto request = queue.front();
            queue.pop();
            return request;
        }

        /**
         * Causes `waitGet()` to always return a false request.
         */
        void stop() {
            Guard guard{mutex};
            stopped = true;
            cond.notify_all();
        }

        /**
         * Tells a subscriber's P2P node about all requests in the queue in the
         * order in which they were added. Deletes those requests from the
         * queue.
         *
         * @param[in] node  Subscriber's P2P node
         * @param[in] peer  Associated local peer
         * @see SubP2pNode::missed(ProdIndex, Peer)
         * @see SubP2pNode::missed(DataSegId, Peer)
         */
        void drainTo(SubP2pNode& node, Peer peer) {
            Guard guard{mutex};
            while (!queue.empty()) {
                queue.front().missed(node, peer);
                queue.pop();
            }
        }
    };

    SubP2pNode& node;        ///< Subscriber's P2P node
    RequestQ    requests;    ///< Requests to be sent to remote peer
    RequestQ    requested;   ///< Requests sent to remote peer
    const bool  rmtIsPub;    ///< Remote peer is publisher's

    /**
     * Connects this instance to its remote peer. Exchanges protocol version and
     * reads the type of the remote peer (publisher or subscriber).
     *
     * @retval `true`   Instance is connected
     * @retval `false`  Connection lost
     */
    bool connect() {
        bool        success = false;
        TcpClntSock socks[3]; // RAII sockets => sockets close on error

        /*
         * Connect to Peer server. Keep consonant with `PeerSrvr::accept()`.
         */

        socks[0] = TcpClntSock{rmtSockAddr};

        const in_port_t noticePort = socks[0].getLclPort();

        if (socks[0].write(noticePort)) {
            socks[1] = TcpClntSock{rmtSockAddr};

            if (socks[1].write(noticePort)) {
                socks[2] = TcpClntSock{rmtSockAddr};

                if (socks[2].write(noticePort)) {
                    assignClntSocks(socks); // Might throw
                    success = xchgProtoVers();
                }
            }
        }

        return success;
    }

    /**
     * Pushes a request onto the request queue.
     *
     * @tparam    ID          Type of `id`
     * @param[in] id          Identifier of request
     * @retval    `false`     No connection. Connection was lost or `start()`
     *                        wasn't called.
     * @retval    `true`      Success
     * @see       `start()`
     */
    template<class ID>
    bool request(const ID id) {
        if (!connected)
            return false;

        LOG_DEBUG("Pushing request");
        const auto size = requests.push(Request{id});
        LOG_DEBUG("Request queue size is " + std::to_string(size));

        return true;
    }
    bool request(const ProdIndex prodIndex) {
        return request<ProdIndex>(prodIndex);
    }
    bool request(const DataSegId segId) {
        return request<DataSegId>(segId);
    }

    /**
     * Reads from the request queue and writes requests to the request socket.
     * Doesn't return until the request queue returns a false request, the
     * connection to the remote peer is lost, or an exception is thrown.
     *
     * Exception are not propagated.
     *
     * @param[in] peer      Associated local peer
     * @see       `RequestQ::stop()`
     */
    void runRequester(Peer peer) noexcept {
        try {
            while (connected) {
                auto request = requests.waitGet();

                if (!request)
                    break;

                requested.push(request);

                if (request.id == Request::Id::PROD_INDEX) {
                    LOG_DEBUG("Sending product information request to %s",
                            requestXprt.to_string().data());
                    connected = requestXprt.send(PduId::PROD_INFO_REQUEST,
                            request.prodIndex);
                }
                else if (request.id == Request::Id::DATA_SEG_ID) {
                    LOG_DEBUG("Sending data segment request to %s",
                            requestXprt.to_string().data());
                    connected = requestXprt.send(PduId::DATA_SEG_REQUEST,
                            request.dataSegId);
                }
            }
            LOG_NOTE("Connection %s closed", requestXprt.to_string().data());
        }
        catch (const std::exception& ex) {
            log_error(ex);
            setExPtr();
        }
        connected = false;

        requested.drainTo(node, peer);
        requests.drainTo(node, peer);

        LOG_DEBUG("Terminating");
    }

    /**
     * Receives a notice from the remote peer about its path to the publisher
     * and notifies the P2P node.
     *
     * @param[in] xprt     Transport
     * @param[in] peer     Associated local peer
     * @retval    `true`   Success
     * @retval    `false`  Lost connection
     * @see       `SubP2pNode::recvNotice(PubPath, SubPeer)`
     */
    bool rcvPubPathNotice(Xprt& xprt, Peer peer) {
        bool notice;
        if (noticeXprt.read(notice)) {
            node.recvNotice(PubPath(notice), peer);
            rmtPubPath = notice;
            return true;
        }
        return false;
    }

    /**
     * Receives a notice about available data. Notifies the subscriber's P2P
     * node. Adds a request for the data to the request-queue if indicated by
     * the P2P node.
     *
     * @tparam    TYPE     Type of notice
     * @param[in] xprt     Transport
     * @param[in] peer     Associated local peer
     * @retval    `false`  Connection lost
     * @retval    `true`   Success
     */
    template<class TYPE>
    bool rcvNotice(Xprt& xprt, Peer peer) {
        TYPE notice;
        if (!notice.read(xprt))
            return false;
        if (!node.recvNotice(notice, peer))
            return true;
        return request(notice);
    }

    /**
     * Receives data from the remote peer. Passes the data to the subscriber's
     * P2P node. Removes the associated request from the requested queue.
     * If the data wasn't requested, then it is ignored and each request in the
     * requested queue is removed from the queue and the P2P node is notified
     * that it cannot be satisfied by the remote peer (NB: the requested queue
     * is emptied).
     *
     * @tparam    DATA     Type of data (`ProdInfo`, `DataSeg`)
     * @param[in] xprt     Transport
     * @param[in] peer     Associated local peer
     * @retval    `true`   Still connected
     * @retval    `false`  Lost Connection
     * @see `Request::missed()`
     */
    template<class DATA>
    bool rcvData(Xprt& xprt, Peer peer) {
        bool success = false;
        DATA data{};
        if (data.read(dataXprt)) {
            while (!requested.empty()) {
                const auto& request = requested.front();
                if (request.requested(data)) {
                    node.recvData(data, peer);
                    requested.pop();
                    break;
                } // Found request for this data
                /*
                 *  NB: A missed response means that the remote peer doesn't
                 *  have the requested data.
                 */
                request.missed(node, peer);
                requested.pop();
            }
            success = true;
        }
        return success;
    }

protected:
    std::atomic<bool>  rmtPubPath;

    void startThreads(Peer peer) override {
        noticeReader = Thread(&SubPeerImpl::runReader, this, noticeXprt,
                [=](Xprt::PduId pduId, Xprt xprt) {
                        return processNotice(pduId, peer);});

        try {
            // Publisher's don't make requests
            if (!rmtIsPub)
                requestReader = Thread(&SubPeerImpl::runReader, this, requestXprt,
                    [=](Xprt::PduId pduId, Xprt xprt) {
                            return processRequest(pduId, peer);});

            try {
                dataReader = Thread(&SubPeerImpl::runReader, this, dataXprt,
                    [=](Xprt::PduId pduId, Xprt xprt) {
                            return processData(pduId, peer);});

                try {
                    requestWriter = Thread(&SubPeerImpl::runRequester, this, peer);
                }
                catch (const std::exception& ex) {
                    if (dataReader.joinable()) {
                        ::pthread_cancel(dataReader.native_handle());
                        dataReader.join();
                    }
                    throw;
                } // `dataReader` created
            }
            catch (const std::exception& ex) {
                if (requestReader.joinable()) {
                    ::pthread_cancel(requestReader.native_handle());
                    requestReader.join();
                }
                throw;
            }
        } // `noticeReader` created
        catch (const std::exception& ex) {
            if (noticeReader.joinable()) {
                ::pthread_cancel(noticeReader.native_handle());
                noticeReader.join();
            }
            throw;
        }
    }

    bool processNotice(Xprt::PduId pduId, Peer peer) {
        bool success = false; // EOF default

        if (PduId::DATA_SEG_NOTICE == pduId) {
            LOG_NOTE("Reading data-segment notice from %s",
                    noticeXprt.to_string().data());
            success = rcvNotice<DataSegId>(noticeXprt, peer);
        }
        else if (PduId::PROD_INFO_NOTICE == pduId) {
            LOG_NOTE("Reading product-information notice from %s",
                    noticeXprt.to_string().data());
            success = rcvNotice<ProdIndex>(noticeXprt, peer);
        }
        else if (PduId::PUB_PATH_NOTICE == pduId) {
            success = rcvPubPathNotice(noticeXprt, peer);
        }
        else {
            LOG_DEBUG("Unknown PDU ID: %d", (int)pduId);
            throw LOGIC_ERROR("Invalid PDU type: " + std::to_string(pduId));
        }

        return success;
    }

    /**
     * Processes incoming data from remote peer.
     *
     * @param[in] pduId    Protocol data unit identifier
     * @param[in] peer     Associated local peer
     * @retval    `true`   Success
     * @retval    `false`  Connection lost
     */
    bool processData(Xprt::PduId pduId, Peer peer) {
        bool success = false;

        if (PduId::DATA_SEG == pduId) {
            LOG_NOTE("Reading data-segment from %s",
                    dataXprt.to_string().data());
            success = rcvData<DataSeg>(dataXprt, peer);
        }
        else if (PduId::PROD_INFO == pduId) {
            LOG_NOTE("Reading product-information from %s",
                    dataXprt.to_string().data());
            success = rcvData<ProdInfo>(dataXprt, peer);
        }
        else if (PduId::PEER_SRVR_ADDRS == pduId) {
            LOG_NOTE("Reading potential peer-servers from %s",
                    dataXprt.to_string().data());
            PeerSrvrAddrs peerSrvrAddrs{};
            success = peerSrvrAddrs.read(dataXprt);
            if (success)
                node.recvData(peerSrvrAddrs, peer);
        }
        else {
            LOG_DEBUG("Unknown PDU ID: %d", (int)pduId);
            throw LOGIC_ERROR("Invalid PDU type: " + std::to_string(pduId));
        }

        return success;
    }

    /**
     * Idempotent.
     */
    void stopAndJoinThreads() override {
        state = State::STOPPING;
        requests.stop();

        if (dataXprt)
            dataXprt.shutdown();
        if (requestXprt)
            requestXprt.shutdown();
        if (noticeXprt)
            noticeXprt.shutdown();

        if (requestWriter.joinable())
            requestWriter.join();
        if (dataReader.joinable())
            dataReader.join();
        if (requestReader.joinable())
            requestReader.join();
        if (noticeReader.joinable())
            noticeReader.join();
    }

public:
    /**
     * Client-side construction.
     *
     * @param[in] node          Subscriber's P2P node
     * @param[in] srvrAddr      Socket address of remote peer-server
     * @param[in] rmtIsPub      Remote peer-server is publisher's
     * @throw     RuntimeError  Couldn't connect
     */
    SubPeerImpl(SubP2pNode& node,
            const SockAddr  srvrAddr,
            const bool      rmtIsPub)
        : Impl(node, true)
        , node(node)
        , requests()
        , requested()
        , rmtPubPath(false)
        , rmtIsPub(rmtIsPub)
    {
        rmtSockAddr = srvrAddr;
        if (!connect()) // Calls `xchgProtoVers()`
            throw RUNTIME_ERROR("Couldn't connect to " +
                    rmtSockAddr.to_string());
        connected = true;
    }

    /**
     * Server-side construction.
     *
     * @param[in] node   Subscriber's P2P node
     * @param[in] socks  Sockets connected to the remote peer
     */
    SubPeerImpl(SubP2pNode& node, TcpSock socks[3])
        : Impl(node, false)
        , node(node)
        , requests()
        , requested()
        , rmtPubPath(false)
        , rmtIsPub(false)
    {
        assignSrvrSocks(socks);
        rmtSockAddr = noticeXprt.getRmtAddr();
        if (!xchgProtoVers())
            throw RUNTIME_ERROR("Lost connection with " +
                    rmtSockAddr.to_string());
        connected = true;
    }

    bool rmtIsPubPath() const noexcept override {
        return rmtPubPath;
    }
};

SubPeer::SubPeer(SubP2pNode& node, TcpSock socks[3])
    : Peer(new SubPeerImpl(node, socks))
{}

SubPeer::SubPeer(SubP2pNode&     node,
                 const SockAddr& srvrAddr,
                 const bool      rmtIsPub)
    : Peer(new SubPeerImpl(node, srvrAddr, rmtIsPub))
{}

/******************************************************************************/

/**
 * Peer-server implementation. Listens for connections from remote, subscribing,
 * client-side peers and creates an associated local, server-side peer.
 *
 * @tparam NODE  Type of P2P node: `P2pNode` or `SubP2pNode`
 * @tparam PEER  Type of associated peer: `PubPeer` or `SubPeer`
 */
template<class NODE, class PEER>
class PeerSrvr<NODE, PEER>::Impl
{
    /**
     * Queue of connected local peers.
     */
    using PeerQ = std::queue<PEER, std::list<PEER>>;

    /**
     * Factory for creating associated connected local peers.
     */
    class PeerFactory
    {
        struct Hash {
            size_t operator()(const SockAddr& sockAddr) const {
                return sockAddr.hash();
            };
        };

        struct AreEqual {
            bool operator()(const SockAddr& lhs, const SockAddr& rhs) const {
                return lhs == rhs;
            };
        };

        using Map = std::unordered_map<SockAddr, TcpSock[3], Hash, AreEqual>;

        NODE& node;
        Map   connections;

    public:
        /**
         * Constructs.
         *
         * @tparam    NODE  Type of P2P node (publisher or subscriber)
         * @param[in] node  P2P node
         */
        PeerFactory(NODE& node)
            : node(node)
            , connections()
        {}

        /**
         * Adds an individual socket to a server-side peer. If the addition
         * completes the entry, then it removed from this instance.
         *
         * @param[in]  sock        Individual socket
         * @param[in]  noticePort  Port number of the notification socket
         * @param[out] peer        Associated peer. Set iff `true` is returned.
         * @retval     `false`     Peer is not complete. `peer` is not set.
         * @retval     `true`      Peer is complete. `peer` is set.
         * @threadsafety           Unsafe
         */
        bool add(TcpSock& sock, in_port_t noticePort, PEER& peer) {
            // TODO: Limit number of outstanding connections
            // TODO: Purge old entries
            auto key = sock.getRmtAddr().clone(noticePort);
            auto socks = connections[key];

            for (int i = 0; i < 3; ++i) {
                if (!socks[i]) {
                    socks[i] = sock;
                    break;
                }
            }

            if (!socks[2])
                return false;

            peer = PEER(node, socks);
            connections.erase(key);
            return true;
        }
    };

    mutable Mutex     mutex;
    mutable Cond      cond;
    PeerFactory       peerFactory;
    const TcpSrvrSock srvrSock;     /// Socket on which this instance listens
    PeerQ             acceptQ;      /// Queue of associated local peers
    size_t            maxAccept;    /// Maximum size of the peer queue

    /**
     * Executes on a separate thread.
     *
     * @param[in] sock  Newly-accepted socket
     */
    void acceptSock(TcpSock sock) {
        in_port_t noticePort;

        if (sock.read(noticePort)) { // Might take a while
            // The rest is fast
            Guard guard{mutex};

            if (acceptQ.size() < maxAccept) {
                PEER peer{};
                if (peerFactory.add(sock, noticePort, peer))
                    acceptQ.push(peer);
                cond.notify_one();
            }
        } // Read notice port number
    }

public:
    /**
     * Constructs from the local address for the peer-server.
     *
     * @tparam    NODE       Type of P2P node (publisher or subscriber)
     * @param[in] node       P2P node
     * @param[in] srvrAddr   Local Address for peer server
     * @param[in] maxAccept  Maximum number of outstanding peer connections
     */
    Impl(   NODE&          node,
            const SockAddr srvrAddr,
            const unsigned maxAccept)
        : mutex()
        , cond()
        , peerFactory(node)
        , srvrSock(srvrAddr, 3*maxAccept)
        , acceptQ()
        , maxAccept(maxAccept)
    {}

    SockAddr getSockAddr() const {
        return srvrSock.getLclAddr();
    }

    /**
     * Returns the next local peer.
     *
     * @return              Next local peer.
     * @throws SystemError  `::accept()` failure
     */
    PEER accept() {
        Lock lock{mutex};

        while (acceptQ.empty()) {
            // TODO: Limit number of threads
            // TODO: Lower priority of thread to favor data transmission
            auto sock = srvrSock.accept();
            if (sock)
                Thread(&Impl::acceptSock, this, sock).detach();
            cond.wait(lock);
        }

        auto peer = acceptQ.front();
        acceptQ.pop();

        return peer;
    }
};

template<>
PeerSrvr<P2pNode,PubPeer>::PeerSrvr(P2pNode&       node,
                                    const SockAddr srvrAddr,
                                    unsigned       maxAccept)
    : pImpl(new Impl(node, srvrAddr, maxAccept))
{}

template<>
SockAddr PeerSrvr<P2pNode,PubPeer>::getSockAddr() const {
    return pImpl->getSockAddr();
}

template<>
PubPeer PeerSrvr<P2pNode,PubPeer>::accept() {
    return pImpl->accept();
}

template<>
PeerSrvr<SubP2pNode,SubPeer>::PeerSrvr(SubP2pNode&    node,
                                       const SockAddr srvrAddr,
                                       unsigned       maxAccept)
    : pImpl(new Impl(node, srvrAddr, maxAccept))
{}

template<>
SockAddr PeerSrvr<SubP2pNode,SubPeer>::getSockAddr() const {
    return pImpl->getSockAddr();
}

template<>
SubPeer PeerSrvr<SubP2pNode,SubPeer>::accept() {
    return pImpl->accept();
}

} // namespace
