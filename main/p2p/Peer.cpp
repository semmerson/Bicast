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
#include <list>
#include <queue>
#include <unordered_map>
#include <utility>

namespace hycast {

/**
 * Implementation of the `Peer` class.
 */
class Peer::Impl
{
    friend class PubImpl;

protected:
    mutable Mutex      sockMutex;
    mutable Mutex      rmtSockAddrMutex;
    mutable Mutex      exceptMutex;
    P2pNode*           node;
    /*
     * If a single socket is used for asynchronous communication and reading and
     * writing occur on the same thread, then deadlock will occur if both
     * receive buffers are full and each end is trying to write. To prevent
     * this, three sockets are used and a thread that reads from one socket will
     * write to another. The pipeline might block for a while as messages are
     * processed, but it won't deadlock.
     */
    TcpSock            noticeSock;
    TcpSock            requestSock;
    TcpSock            dataSock;
    Thread             noticeReader;
    Thread             requestReader;
    Thread             dataReader;
    Thread             requestWriter;
    SockAddr           rmtSockAddr;
    enum class State {
        INITED,
        STARTING,
        STARTED,
        STOPPING
    };
    using AtomicState = std::atomic<State>;
    AtomicState        state;
    std::exception_ptr exPtr;
    std::atomic<bool>  connected;
    using AtomicNodeType = std::atomic<P2pNode::Type>;
    AtomicNodeType     rmtNodeType;

    Impl() =default;

    /**
     * Orders the sockets so that the notice socket has the lowest client-side
     * port number, then the request socket, and then the data socket.
     *
     * @param[in] notSock  Notice socket
     * @param[in] reqSock  Request socket
     * @param[in] datSock  Data socket
     */
    virtual void orderSocks(TcpSock& notSock,
                            TcpSock& reqSock,
                            TcpSock& datSock) =0;

    static inline bool write(TcpSock& sock, const PduId id) {
        return sock.write(static_cast<PduType>(id));
    }

    static inline bool read(TcpSock& sock, PduId& pduId) {
        PduType id;
        auto    success = sock.read(id);
        pduId = static_cast<PduId>(id);
        return success;
    }

    static inline bool write(TcpSock& sock, const P2pNode::Type nodeType) {
        return sock.write(static_cast<uint32_t>(nodeType));
    }

    static inline bool read(TcpSock& sock, P2pNode::Type& nodeType) {
        uint32_t type;
        if (sock.read(type)) {
            nodeType = static_cast<P2pNode::Type>(type);
            return true;
        }
        return false;
    }

    static inline bool write(TcpSock& sock, const ProdIndex index) {
        return sock.write((ProdIndex::Type)index);
    }

    static inline bool read(TcpSock& sock, ProdIndex& index) {
        ProdIndex::Type i;
        if (sock.read(i)) {
            index = ProdIndex(i);
            return true;
        }
        return false;
    }

    static inline bool write(TcpSock& sock, const Timestamp& timestamp) {
        return sock.write(timestamp.sec) && sock.write(timestamp.nsec);
    }

    static inline bool read(TcpSock& sock, Timestamp& timestamp) {
        return sock.read(timestamp.sec) && sock.read(timestamp.nsec);
    }

    static inline bool write(TcpSock& sock, const DataSegId& id) {
        return write(sock, id.prodIndex) && sock.write(id.offset);
    }

    static inline bool write(TcpSock& sock, const ProdInfo& prodInfo) {
        return write(sock, prodInfo.getIndex()) &&
                sock.write(prodInfo.getName()) &&
                sock.write(prodInfo.getSize());
    }

    static bool read(TcpSock& sock, ProdInfo& prodInfo) {
        ProdIndex index;
        String    name;
        ProdSize  size;

        if (!read(sock, index) ||
               !sock.read(name) ||
               !sock.read(size))
            return false;

        prodInfo = ProdInfo(index, name, size);
        return true;
    }

    static inline bool write(TcpSock& sock, const DataSeg& dataSeg) {
        return write(sock, dataSeg.getId()) &&
                sock.write(dataSeg.getProdSize()) &&
                sock.write(dataSeg.getData(), dataSeg.getSize());
    }

    static inline bool read(TcpSock& sock, DataSegId& id) {
        return read(sock, id.prodIndex) && sock.read(id.offset);
    }

    static inline bool read(TcpSock& sock, bool& value) {
        return sock.read(value);
    }

    static bool read(TcpSock& sock, DataSeg& dataSeg) {
        bool success = false;
        DataSegId id;
        ProdSize  size;
        if (read(sock, id) && sock.read(size)) {
            dataSeg = DataSeg(id, size, sock);
            success = true;;
        }
        return success;
    }

    void setExPtr() {
        Guard guard{exceptMutex};
        if (!exPtr)
            exPtr = std::current_exception();
    }

    /**
     * Throws an exception if the state of this instance isn't appropriate or an
     * exception has been thrown by one of this instance's threads.
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

    virtual bool ensureConnected() =0;

    /**
     * Sends data to the remote peer.
     *
     * @retval    `false`     No connection. Connection was lost or `start()`
     *                        wasn't called.
     * @retval    `true`      Success
     */
    bool send(const ProdInfo& data) {
        if (!connected)
            return false;

        return connected = write(dataSock, PduId::PROD_INFO) &&
                write(dataSock, data);
    }
    bool send(const DataSeg& data) {
        if (!connected)
            return false;

        return connected = write(dataSock, PduId::DATA_SEG) &&
                write(dataSock, data);
    }

    virtual bool rcvPubPathNotice(Peer peer) =0;
    virtual bool rcvProdInfoNotice(Peer peer) =0;
    virtual bool rcvDataSegNotice(Peer peer) =0;

    bool rcvProdInfoRequest(Peer peer) {
        bool      success = false;
        ProdIndex request;
        LOG_DEBUG("Receiving product information request");
        if (read(requestSock, request)) {
            auto prodInfo = node->recvRequest(request, peer);
            auto success = !prodInfo || send(prodInfo);
            if (success) LOG_DEBUG("Product information sent");
        }
        return success;
    }
    bool rcvDataSegRequest(Peer peer) {
        bool      success;
        DataSegId request;
        LOG_DEBUG("Receiving data segment request");
        if (read(requestSock, request)) {
            auto dataSeg = node->recvRequest(request, peer);
            success = !dataSeg || send(dataSeg);
            if (success) LOG_DEBUG("Data segment sent");
        }
        return success;
    }

    virtual bool rcvProdInfo(Peer peer) =0;
    virtual bool rcvDataSeg(Peer peer) =0;

    /**
     * Dispatch function for processing an incoming message from the remote
     * peer.
     *
     * @param[in] id            Message type
     * @param[in] peer          Associated local peer
     * @retval    `false`       End-of-file encountered.
     * @retval    `true`        Success
     * @throw std::logic_error  `id` is unknown
     */
    bool processPdu(const PduId id, Peer peer) {
        bool success = false; // EOF default

        switch (id) {
        case PduId::PUB_PATH_NOTICE: {
            success = rcvPubPathNotice(peer);
            break;
        }
        case PduId::PROD_INFO_NOTICE: {
            success = rcvProdInfoNotice(peer);
            break;
        }
        case PduId::DATA_SEG_NOTICE: {
            success = rcvDataSegNotice(peer);
            break;
        }
        case PduId::PROD_INFO_REQUEST: {
            success = rcvProdInfoRequest(peer);
            break;
        }
        case PduId::DATA_SEG_REQUEST: {
            success = rcvDataSegRequest(peer);
            break;
        }
        case PduId::PROD_INFO: {
            success = rcvProdInfo(peer);
            break;
        }
        case PduId::DATA_SEG: {
            success = rcvDataSeg(peer);
            break;
        }
        default:
            throw std::logic_error("Invalid PDU type: " +
                    std::to_string(static_cast<PduType>(id)));
        }

        return success;
    }

    /**
     * Reads a socket from the remote peer and processes incoming messages.
     * Doesn't return until either EOF is encountered or an error occurs.
     *
     * @param[in] sock    Socket with remote peer
     * @param[in] peer    Associated local peer
     * @throw LogicError  Message type is unknown
     */
    void runReader(TcpSock sock, Peer peer) {
        try {
            for (;;) {
                PduId id;
                if (!read(sock, id) || !processPdu(id, peer)) {
                    connected = false;
                    break; // EOF
                }
            }
        }
        catch (const std::exception& ex) {
            setExPtr();
        }
    }

    virtual bool exchangeNodeTypes() =0;

    virtual void startThreads(Peer peer) =0;

    virtual void stopAndJoinThreads() =0; // Idempotent

public:
    /**
     * Constructs.
     *
     * @param[in] node      P2P node
     */
    Impl(P2pNode& node)
        : sockMutex()
        , rmtSockAddrMutex()
        , exceptMutex()
        , node(&node)
        , noticeSock()
        , requestSock()
        , dataSock()
        , noticeReader()
        , requestReader()
        , dataReader()
        , requestWriter()
        , rmtSockAddr()
        , state(State::INITED)
        , exPtr()
        , connected{false}
        , rmtNodeType(P2pNode::Type::UNSET)
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

    virtual bool set(TcpSock& sock) =0;
    virtual bool isComplete() const noexcept =0;

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
            connected = ensureConnected();

            if (connected) {
                if (exchangeNodeTypes()) {
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
                } // Node types exchanged
            } // Instance is connected to remote
        } // Instance is initialized

        return success;
    }

    /**
     * Returns the socket address of the remote peer.
     *
     * @return Socket address of remote peer
     */
    SockAddr getRmtAddr() const noexcept {
        Guard guard{rmtSockAddrMutex};
        return rmtSockAddr;
    }

    /**
     * Stops this instance from serving its remote counterpart. Causes the
     * threads serving the remote peer to terminate. If called before `start()`,
     * then the remote peer will not be served.
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

    String to_string(const bool withName) const {
        Guard guard{rmtSockAddrMutex};
        return rmtSockAddr.to_string(withName);
    }

    /**
     * Notifies the remote peer.
     *
     * @retval    `false`     No connection. Connection was lost or `start()`
     *                        wasn't called.
     * @retval    `true`      Success
     */
    bool notify(const P2pNode::Type notice) {
        throwIf();
        if (!connected)
            return false;

        return connected = write(noticeSock, PduId::NODE_TYPE) &&
                write(noticeSock, notice);
    }
    bool notify(const PubPath notice) {
        throwIf();
        if (!connected)
            return false;

        return connected = write(noticeSock, PduId::PUB_PATH_NOTICE) &&
                noticeSock.write(notice.operator bool());
    }
    bool notify(const ProdIndex notice) {
        throwIf();
        if (!connected)
            return false;

        return connected = write(noticeSock, PduId::PROD_INFO_NOTICE) &&
            write(noticeSock, notice);
    }
    bool notify(const DataSegId& notice) {
        throwIf();
        if (!connected)
            return false;

        return connected = write(noticeSock, PduId::DATA_SEG_NOTICE) &&
            write(noticeSock, notice);
    }

    virtual bool rmtIsPubPath() const noexcept =0;
};

/**
 * Server-side peer implementation. The peer could be a publisher or a
 * subscriber.
 */
class SrvrImpl : virtual public Peer::Impl
{
protected:
    void orderSocks(TcpSock& notSock,
                    TcpSock& reqSock,
                    TcpSock& datSock) override {
        if (reqSock.getRmtPort() < notSock.getRmtPort())
            reqSock.swap(notSock);

        if (datSock.getRmtPort() < reqSock.getRmtPort()) {
            datSock.swap(reqSock);
            if (reqSock.getRmtPort() < notSock.getRmtPort())
                reqSock.swap(notSock);
        }
    }

public:
    SrvrImpl()
        : Impl()
    {}

    virtual ~SrvrImpl() noexcept {
    }

    /**
     * Sets the next, individual socket.
     *
     * @param[in] sock        Relevant socket
     * @retval    `true`      This instance is complete
     * @retval    `false`     This instance is not complete
     * @throw     LogicError  Connection is already complete
     */
    bool set(TcpSock& sock) {
        Guard guard{sockMutex};

        // NB: Keep function consonant with `SubImpl(P2pNode, SockAddr)`

        if (!noticeSock) {
            noticeSock = sock;
            Guard guard{rmtSockAddrMutex};
            rmtSockAddr = noticeSock.getRmtAddr();
        }
        else if (!requestSock) {
            requestSock = sock;
        }
        else if (!dataSock) {
            dataSock = sock;
            orderSocks(noticeSock, requestSock, dataSock);
        }
        else {
            throw LOGIC_ERROR("Server-side P2P connection is already complete");
        }

        return dataSock;
    }

    /**
     * Indicates if instance is complete (i.e., has all individual sockets).
     *
     * @retval `false`  Instance is not complete
     * @retval `true`   Instance is complete
     */
    bool isComplete() const noexcept override {
        Guard guard{sockMutex};
        return dataSock;
    }

    bool ensureConnected() override {
        if (!isComplete())
            throw LOGIC_ERROR("Can't connect a server-side peer");
        return true;
    }

    virtual bool rmtIsPubPath() const noexcept =0;
};

/**
 * Client-side local peer implementation. The local peer will be a subscriber.
 */
class ClntImpl : virtual public Peer::Impl
{
protected:
    void orderSocks(TcpSock& notSock,
                    TcpSock& reqSock,
                    TcpSock& datSock) override {
        if (reqSock.getLclPort() < notSock.getLclPort())
            reqSock.swap(notSock);

        if (datSock.getLclPort() < reqSock.getLclPort()) {
            datSock.swap(reqSock);
            if (reqSock.getLclPort() < notSock.getLclPort())
                reqSock.swap(notSock);
        }
    }

    bool exchangeNodeTypes() override {
        bool          success = false; // Failure default
        P2pNode::Type nodeType;

        // Keep consonant with `PubImpl::exchangeNodeTypes()`
        if (read(noticeSock, nodeType)) {
            rmtNodeType = nodeType;
            // NB: publishers don't care about the type of the remote node
            success = (nodeType == P2pNode::Type::PUBLISHER) ||
                    write(noticeSock, P2pNode::Type::SUBSCRIBER);
        }

        return success;
    }

    /**
     * Connects a client-side peer to a remote peer. Blocks while connecting.
     *
     * @retval    `false`     Remote peer disconnected
     * @retval    `true`      Success
     */
    bool ensureConnected() override {
        bool     success = false;
        SockAddr srvrAddr;

        {
            Guard guard{rmtSockAddrMutex};
            srvrAddr = rmtSockAddr;
        }

        // Connect to Peer server.
        // Keep consonant with `PeerSrvr::accept()`
        TcpClntSock notSock{srvrAddr};

        const in_port_t noticePort = notSock.getLclPort();

        if (notSock.write(noticePort)) {
            TcpClntSock reqSock{srvrAddr};

            if (reqSock.write(noticePort)) {
                TcpClntSock datSock{srvrAddr};

                if (datSock.write(noticePort)) {
                    // Use of local RAII sockets => sockets close on error
                    orderSocks(notSock, reqSock, datSock); // Might throw
                    noticeSock  = notSock;
                    requestSock = reqSock;
                    dataSock    = datSock;
                    success     = true;
                }
            }
        }

        return success;
    }

public:
    /**
     * Constructs.
     *
     * @param[in] srvrAddr  Socket address of peer-server
     */
    ClntImpl(const SockAddr srvrAddr)
    {
        rmtSockAddr = srvrAddr;
    }

    virtual ~ClntImpl() noexcept {
    }

    bool set(TcpSock& sock) {
        throw LOGIC_ERROR("Can't set a subscriber's socket");
    }
    bool isComplete() const noexcept override {
        return true;
    }

    virtual bool rmtIsPubPath() const noexcept =0;
};

/**
 * Publishing peer implementation. It will be server-side constructed.
 */
class PubImpl final : virtual public Peer::Impl, public SrvrImpl
{
protected:
    bool exchangeNodeTypes() override {
        // Keep consonant with `ClntImpl::exchangeNodeTypes()`
        // NB: Publishers don't care about the type of the remote node
        return write(noticeSock, P2pNode::Type::PUBLISHER);
    }

    bool rcvPubPathNotice(Peer peer) override {
        throw LOGIC_ERROR("Publisher shouldn't receive notices");
    }
    bool rcvProdInfoNotice(Peer peer) override {
        throw LOGIC_ERROR("Publisher shouldn't receive notices");
    }
    bool rcvDataSegNotice(Peer peer) override {
        throw LOGIC_ERROR("Publisher shouldn't receive notices");
    }

    bool rcvProdInfo(Peer peer) override {
        throw LOGIC_ERROR("Publisher shouldn't receive data");
    }
    bool rcvDataSeg(Peer peer) override {
        throw LOGIC_ERROR("Publisher shouldn't receive data");
    }

    void startThreads(Peer peer) {
        requestReader = Thread(&PubImpl::runReader, this, requestSock, peer);
    }

    /**
     * Idempotent.
     */
    void stopAndJoinThreads() {
        state = State::STOPPING;

        if (requestSock)
            requestSock.shutdown(SHUT_RD);
        if (requestReader.joinable())
            requestReader.join();
    }

public:
    /**
     * Constructs.
     *
     * @param[in] node      P2P node
     */
    PubImpl(P2pNode& node)
        : Impl(node)
        , SrvrImpl()
    {}

    bool rmtIsPubPath() const noexcept override {
        return true; // Because it's connected to me
    }
};

/**
 * Subscribing peer implementation. May be server-side or client-side
 * constructed.
 */
class SubImpl : virtual public Peer::Impl
{
    friend class Request;

    /**
     * Class for requests sent to a remote peer. Implemented as a discriminated
     * union so that it has a known size and, consequently, can be an element in
     * a container.
     */
    class Request {
        enum class Id {
            UNSET,
            PROD_INFO,
            DATA_SEG
        } id;
        union {
            ProdIndex prodIndex;
            DataSegId dataSegId;
        };

    public:
        Request()
            : prodIndex()
            , id(Id::UNSET)
        {}

        Request(const Request& request) =default;
        Request(Request&& request) =default;

        Request(const ProdIndex prodIndex)
            : id(Id::PROD_INFO)
            , prodIndex(prodIndex)
        {}

        Request(const DataSegId dataSegId)
            : id(Id::DATA_SEG)
            , dataSegId(dataSegId)
        {}

        Request& operator=(const Request& request) =default;

        Request& operator=(Request&& request) =default;

        operator bool() const noexcept {
            return id != Id::UNSET;
        }

        String to_string() const {
            return id == Id::PROD_INFO
                    ? prodIndex.to_string()
                    : id == Id::DATA_SEG
                          ? dataSegId.to_string()
                          : "<unset>";
        }

        void missed(P2pNode& node, Peer peer) const {
            if (id == Id::PROD_INFO) {
                node.missed(prodIndex, peer);
            }
            else if (id == Id::DATA_SEG) {
                node.missed(dataSegId, peer);
            }
            else {
                throw LOGIC_ERROR("Invalid ID: " + std::to_string((int)id));
            }
        }

        bool equals(const ProdIndex prodIndex) const noexcept {
            return id == Id::PROD_INFO && this->prodIndex == prodIndex;
        }

        bool equals(const DataSegId& dataSegId) const noexcept {
            return id == Id::DATA_SEG && this->dataSegId == dataSegId;
        }

        bool write(TcpSock& sock) const {
            bool success;

            if (id == Id::PROD_INFO) {
                success = sock.write(
                            static_cast<PduType>(PduId::PROD_INFO_REQUEST))
                        && sock.write((ProdIndex::Type)prodIndex);
            }
            else if (id == Id::DATA_SEG) {
                success = sock.write(
                            static_cast<PduType>(PduId::DATA_SEG_REQUEST))
                        && sock.write((ProdIndex::Type)dataSegId.prodIndex)
                        && sock.write(dataSegId.offset);
            }
            else {
                throw LOGIC_ERROR("Instance is unset");
            }

            return success;
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

        bool empty() const {
            Guard guard{mutex};
            return queue.empty();
        }

        /**
         * Returns the request at the head of the queue.
         *
         * @return  Head request
         */
        Request& front() {
            Guard guard{mutex};
            return queue.front();
        }

        /**
         * Deletes the request at the head of the queue.
         */
        void pop() {
            Guard guard{mutex};
            queue.pop();
        }

        /**
         * Removes and returns the request at the head of the queue. Blocks
         * until it exists or `stop()` is called.
         *
         * @return  Head request or false one. Will test false if `stop()` was
         *          called.
         * @see `stop()`
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

        void drainTo(P2pNode& node, Peer peer) {
            Guard guard{mutex};
            while (!queue.empty()) {
                queue.front().missed(node, peer);
                queue.pop();
            }
        }
    };
    RequestQ           requests;
    RequestQ           requested;

protected:
    std::atomic<bool>  rmtPubPath;

    SubImpl()
        : requests()
        , requested()
        , rmtPubPath(false)
    {}

    bool rcvPubPathNotice(Peer peer) override {
        bool notice;
        if (read(noticeSock, notice)) {
            node->recvNotice(PubPath(notice), peer);
            rmtPubPath = notice;
            return true;
        }
        return false;
    }
    bool rcvProdInfoNotice(Peer peer) override {
        ProdIndex notice;
        return read(noticeSock, notice) && node->recvNotice(notice, peer) &&
            request(notice);
    }
    bool rcvDataSegNotice(Peer peer) override {
        DataSegId notice;
        return read(noticeSock, notice) && node->recvNotice(notice, peer) &&
            request(notice);
    }
    bool rcvProdInfo(Peer peer) override {
        bool success = false;
        ProdInfo       data;
        if (read(dataSock, data)) {
            while (!requested.empty()) {
                const auto& expected = requested.front();
                if (expected.equals(data.getIndex())) {
                    node->recvData(data, peer);
                    requested.pop();
                    break;
                }

                expected.missed(*node, peer);
                requested.pop();
            }
            success = true;
        }
        return success;
    }
    bool rcvDataSeg(Peer peer) override {
        bool success = false;
        DataSeg        data;
        if (read(dataSock, data)) {
            while (!requested.empty()) {
                const auto& expected = requested.front();
                if (expected.equals(data.getId())) {
                    node->recvData(data, peer);
                    requested.pop();
                    break;
                }

                expected.missed(*node, peer);
                requested.pop();
            }
            success = true;
        }
        return success;
    }

    void runRequester(Peer peer) {
        try {
            for (;;) {
                const auto request = requests.waitGet();

                if (!request) {
                    // `requests.stop()` called
                    requested.drainTo(*node, peer);
                    requests.drainTo(*node, peer);
                    break;
                }

                requested.push(request);

                if (!request.write(requestSock)) {
                    connected = false;
                    break; // Connection lost
                }
            }
        }
        catch (const std::exception& ex) {
            setExPtr();
        }

        LOG_DEBUG("Terminating");
    }

    void startThreads(Peer peer) {
        noticeReader = Thread(&SubImpl::runReader, this, noticeSock, peer);

        try {
            if (rmtNodeType != P2pNode::Type::PUBLISHER)
                requestReader = Thread(&SubImpl::runReader, this, requestSock,
                        peer);

            try {
                dataReader = Thread(&SubImpl::runReader, this, dataSock, peer);

                try {
                    requestWriter = Thread(&SubImpl::runRequester, this, peer);
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

    /**
     * Requests product information from the remote peer. Blocks while writing.
     *
     * @param[in] prodIndex   Index of product
     * @retval    `false`     No connection. Connection was lost or `start()`
     *                        wasn't called.
     * @retval    `true`      Success
     */
    bool request(const ProdIndex prodIndex) {
        if (!connected)
            return false;

        LOG_DEBUG("Pushing product information request");
        const auto size = requests.push(Request(prodIndex));
        LOG_DEBUG("Request queue size is " + std::to_string(size));

        return true;
    }

    /**
     * Requests a data-segment from the remote peer. Blocks while writing.
     *
     * @param[in] segId       ID of data-segment
     * @retval    `false`     Remote peer disconnected
     * @retval    `true`      Success
     */
    bool request(const DataSegId& segId) {
        if (!connected)
            return false;

        LOG_DEBUG("Pushing data segment request");
        const auto size = requests.push(Request(segId));
        LOG_DEBUG("Request queue size is " + std::to_string(size));

        return true;
    }

    /**
     * Idempotent.
     */
    void stopAndJoinThreads() {
        state = State::STOPPING;
        requests.stop();

        if (dataSock)
            dataSock.shutdown(SHUT_RD);
        if (requestSock)
            requestSock.shutdown(SHUT_RD);
        if (noticeSock)
            noticeSock.shutdown(SHUT_RD);

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
    virtual ~SubImpl() {
    }

    //virtual bool rmtIsPubPath() const noexcept =0;

    bool rmtIsPubPath() const noexcept override {
        return rmtPubPath;
    }
};

/**
 * Server-side constructed subscribing peer implementation.
 */
class SrvrSubImpl final : public SrvrImpl, public SubImpl
{
protected:
    bool exchangeNodeTypes() override {
        bool          success = false; // Failure default
        P2pNode::Type nodeType;

        // Keep consonant with `ClntImpl::exchangeNodeTypes()`
        // NB: The remote node will be a subscriber
        if (write(noticeSock, P2pNode::Type::SUBSCRIBER) &&
                read(noticeSock, nodeType)) {
            rmtNodeType = nodeType;
            success = true;
        }

        return success;
    }

public:
    /**
     * Constructs.
     *
     * @param[in] node      P2P node
     */
    SrvrSubImpl(P2pNode& node)
        : Impl(node)
        , SrvrImpl()
    {}

    bool rmtIsPubPath() const noexcept override {
        return rmtPubPath;
    }
};

/**
 * Client-side constructed subscribing peer implementation.
 */
class ClntSubImpl final : public ClntImpl, public SubImpl
{
protected:
    bool exchangeNodeTypes() override {
        bool          success = false; // Failure default
        P2pNode::Type nodeType;

        // Keep consonant with `ClntImpl::exchangeNodeTypes()`
        // NB: The remote node will be a subscriber
        if (write(noticeSock, P2pNode::Type::SUBSCRIBER) &&
                read(noticeSock, nodeType)) {
            rmtNodeType = nodeType;
            if (nodeType == P2pNode::Type::PUBLISHER)
                rmtPubPath = true;
            success = true;
        }

        return success;
    }
public:
    /**
     * Constructs.
     *
     * @param[in] node      P2P node
     * @param[in] srvrAddr  Socket address of peer-server
     */
    ClntSubImpl(P2pNode& node, const SockAddr srvrAddr)
        : Impl(node)
        , ClntImpl(srvrAddr)
    {}

    bool rmtIsPubPath() const noexcept override {
        return rmtPubPath;
    }
};

/******************************************************************************/

Peer::Peer(SharedPtr& pImpl)
    : pImpl(pImpl)
{}

Peer::Peer(P2pNode& node)
    : pImpl(node.isPublisher()
            ? static_cast<Impl*>(new PubImpl(node))
            : static_cast<Impl*>(new SrvrSubImpl(node)))
{}

Peer::Peer(P2pNode& node, const SockAddr& srvrAddr)
    : pImpl(std::make_shared<ClntSubImpl>(node, srvrAddr))
{}

bool Peer::set(TcpSock& sock) {
    return pImpl->set(sock);
}

bool Peer::isComplete() const noexcept {
    return pImpl->isComplete();
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

String Peer::to_string(const bool withName) const {
    return pImpl->to_string(withName);
}

bool Peer::notify(const PubPath notice) const {
    return pImpl->notify(notice);
}

bool Peer::notify(const ProdIndex notice) const {
    return pImpl->notify(notice);
}

bool Peer::notify(const DataSegId& notice) const {
    return pImpl->notify(notice);
}

bool Peer::rmtIsPubPath() const noexcept {
    pImpl->rmtIsPubPath();
}

/******************************************************************************/

/**
 * Peer-server implementation.
 */
class PeerSrvr::Impl
{
    using PeerQ = std::queue<Peer, std::list<Peer>>;

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

        using Map = std::unordered_map<SockAddr, Peer, Hash, AreEqual>;

        P2pNode& node;
        Map      peers;

    public:
        PeerFactory(P2pNode& node)
            : node(node)
            , peers()
        {}

        /**
         * Adds an individual socket to a server-side peer. If the addition
         * completes the entry, then it removed from this instance.
         *
         * @param[in] sock        Individual socket
         * @param[in] noticePort  Port number of the notification socket
         * @return                Associated peer
         * @threadsafety          Unsafe
         */
        Peer add(TcpSock& sock, in_port_t noticePort) {
            // TODO: Limit number of outstanding connections
            // TODO: Purge old entries
            auto key = sock.getRmtAddr().clone(noticePort);
            auto peer = peers[key];

            if (!peer)
                peers[key] = peer = Peer(node);

            if (peer.set(sock))
                peers.erase(key);

            return peer;
        }
    };

    mutable Mutex     mutex;
    mutable Cond      cond;
    PeerFactory       peerFactory;
    const TcpSrvrSock srvrSock;
    PeerQ             acceptQ;
    PeerQ::size_type  maxAccept;

    /**
     * Executes on separate thread.
     *
     * @param[in] sock  Newly-accepted socket
     */
    void acceptSock(TcpSock sock) {
        in_port_t noticePort;

        if (sock.read(noticePort)) { // Might take a while
            // The rest is fast
            Guard guard{mutex};

            if (acceptQ.size() < maxAccept) {
                auto peer = peerFactory.add(sock, noticePort);
                if (peer.isComplete())
                    acceptQ.push(peer);
                cond.notify_one();
            }
        }
    }

public:
    /**
     * Constructs from the local address of the server.
     *
     * @param[in] node       P2P node
     * @param[in] srvrAddr   Local Address of P2P server
     * @param[in] maxAccept  Maximum number of outstanding P2P connections
     */
    Impl(   P2pNode&        node,
            const SockAddr& srvrAddr,
            const unsigned  maxAccept)
        : mutex()
        , cond()
        , peerFactory(node)
        , srvrSock(srvrAddr, 3*maxAccept)
        , acceptQ()
        , maxAccept(maxAccept)
    {}

    /**
     * Returns the next, accepted, peer-to-peer connection.
     *
     * @return Next P2P connection
     */
    Peer accept() {
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

PeerSrvr::PeerSrvr(P2pNode&        node,
                   const SockAddr& srvrAddr,
                   const unsigned  maxAccept)
    : pImpl(std::make_shared<Impl>(node, srvrAddr, maxAccept))
{}

Peer PeerSrvr::accept() {
    return pImpl->accept();
}

} // namespace
