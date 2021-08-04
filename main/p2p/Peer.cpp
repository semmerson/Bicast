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
 * Abstract base implementation of the `Peer` class.
 */
class Peer::Impl
{
    friend class PubImpl;

    void assignSocks(TcpSock socks[3]) {
        noticeSock  = socks[0];
        requestSock = socks[1];
        dataSock    = socks[2];
    }

protected:
    mutable Mutex      sockMutex;
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
    std::atomic<State>          state;
    std::exception_ptr          exPtr;
    std::atomic<bool>           connected;
    std::atomic<P2pNode::Type>  rmtNodeType;

    Impl()
        : sockMutex()
        , exceptMutex()
        , node(nullptr)
    {}

    void orderClntSocks(TcpSock socks[3]) const noexcept {
        if (socks[1].getLclPort() < socks[0].getLclPort())
            socks[1].swap(socks[0]);

        if (socks[2].getLclPort() < socks[1].getLclPort()) {
            socks[2].swap(socks[1]);
            if (socks[1].getLclPort() < socks[0].getLclPort())
                socks[1].swap(socks[0]);
        }
    }

    void orderSrvrSocks(TcpSock socks[3]) const noexcept {
        if (socks[1].getRmtPort() < socks[0].getRmtPort())
            socks[1].swap(socks[0]);

        if (socks[2].getRmtPort() < socks[1].getRmtPort()) {
            socks[2].swap(socks[1]);
            if (socks[1].getRmtPort() < socks[0].getRmtPort())
                socks[1].swap(socks[0]);
        }
    }

    void assignSrvrSocks(TcpSock socks[3]) {
        orderSrvrSocks(socks);
        assignSocks(socks);
    }

    void assignClntSocks(TcpSock socks[3]) {
        orderClntSocks(socks);
        assignSocks(socks);
    }

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
     * @tparam    TYPE     Type of data
     * @param[in] data     Data
     * @param[in] pduId    Protocol Data Unit identifier
     * @retval    `false`  No connection. Connection was lost or `start()`
     *                     wasn't called.
     * @retval    `true`   Success
     */
    template<class TYPE>
    bool send(const TYPE& data, PduId pduId) {
        if (!connected)
            return false;

        return connected = write(dataSock, pduId) && write(dataSock, data);
    }
    inline bool send(const ProdInfo& data) {
        return send<ProdInfo>(data, PduId::PROD_INFO);
    }
    inline bool send(const DataSeg& data) {
        return send<DataSeg>(data, PduId::DATA_SEG);
    }

    /**
     * Receives a request for data from the remote peer. Sends the data to the
     * remote peer if it exists.
     *
     * @tparam    TYPE     Type of request
     * @param[in] peer     Associated local peer
     * @retval    `true`   Data doesn't exist or was successfully sent
     * @retval    `false`  Connection lost
     */
    template<class TYPE, class DATA>
    bool rcvRequest(Peer peer) {
        bool success = false;
        TYPE request;
        LOG_DEBUG("Receiving request");
        if (read(requestSock, request)) {
            auto data = node->recvRequest(request, peer);
            // Data doesn't exist or was successfully sent
            success = !data || send(data);
            if (success) LOG_DEBUG("Product information sent");
        }
        return success;
    }
    inline bool rcvProdInfoRequest(Peer peer) {
        return rcvRequest<ProdIndex, ProdInfo>(peer);
    }
    inline bool rcvDataSegRequest(Peer peer) {
        return rcvRequest<DataSegId, DataSeg>(peer);
    }

    /**
     * Dispatch function for processing an incoming message from the remote
     * peer.
     *
     * @param[in] id            Message type
     * @param[in] peer          Associated local peer
     * @retval    `false`       Connection lost
     * @retval    `true`        Success
     * @throw std::logic_error  `id` is unknown
     */
    virtual bool processPdu(const PduId id, Peer peer) =0;

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

    /**
     * Exchanges node-type information with the remote peer.
     *
     * @retval `true`   Success
     * @retval `false`  Connection lost
     */
    virtual bool exchangeNodeTypes() =0;

    /**
     * Starts the internal threads needed to service messages from the remote
     * peer.
     *
     * @param[in] peer  Associated local peer
     */
    virtual void startThreads(Peer peer) =0;

    /**
     * Stops and joins the threads that are servicing messages from the remote
     * peer.
     */
    virtual void stopAndJoinThreads() =0; // Idempotent

public:
    /**
     * Constructs.
     *
     * @param[in] node      P2P node
     */
    Impl(P2pNode& node)
        : sockMutex()
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

    String to_string() const {
        return rmtSockAddr.to_string();
    }

    /**
     * Notifies the remote peer.
     *
     * @tparam    TYPE     Type of notice
     * @param[in] notice   Notice
     * @param[in] pduId    Notice identifier for message
     * @retval    `false`  No connection. Connection was lost or `start()`
     *                     wasn't called.
     * @retval    `true`   Success
     */
    template<class TYPE>
    bool notify(const TYPE notice, PduId pduId) {
        throwIf();
        if (!connected)
            return false;

        return connected = write(noticeSock, pduId) &&
                write(noticeSock, notice);
    }
    bool notify(const P2pNode::Type notice) {
        return notify<P2pNode::Type>(notice, PduId::NODE_TYPE);
    }
    bool notify(const PubPath notice) {
        return notify<bool>(static_cast<bool>(notice),
                PduId::PUB_PATH_NOTICE);
    }
    bool notify(const ProdIndex notice) {
        return notify<ProdIndex>(notice, PduId::PROD_INFO_NOTICE);
    }
    bool notify(const DataSegId& notice) {
        return notify<DataSegId>(notice, PduId::DATA_SEG_NOTICE);
    }

    virtual bool rmtIsPubPath() const noexcept =0;
};

Peer::Peer(Impl* impl)
    : pImpl(impl)
{}

Peer::Peer(SharedPtr& pImpl)
    : pImpl(pImpl)
{}

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
    return pImpl->to_string();
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
 * Publisher's peer implementation. The peer will be server-side constructed.
 */
class PubImpl : public Peer::Impl
{
protected:
    bool exchangeNodeTypes() override {
        // Keep consonant with `SubImpl::exchangeNodeTypes()`
        // NB: Publishers don't care about the type of the remote node
        return write(noticeSock, P2pNode::Type::PUBLISHER);
    }

    void startThreads(Peer peer) override {
        requestReader = Thread(&PubImpl::runReader, this, requestSock, peer);
    }

    /**
     * Dispatch function for processing an incoming message from the remote
     * peer.
     *
     * @param[in] id            Message type
     * @param[in] peer          Associated local peer
     * @retval    `false`       Connection lost
     * @retval    `true`        Success
     * @throw std::logic_error  `id` is unknown
     */
    bool processPdu(const PduId id, Peer peer) override {
        bool success = false; // EOF default

        switch (id) {
        case PduId::PROD_INFO_REQUEST: {
            success = rcvProdInfoRequest(peer);
            break;
        }
        case PduId::DATA_SEG_REQUEST: {
            success = rcvDataSegRequest(peer);
            break;
        }
        default:
            throw std::logic_error("Invalid PDU type: " +
                    std::to_string(static_cast<PduType>(id)));
        }

        return success;
    }

    /**
     * Idempotent.
     */
    void stopAndJoinThreads() override {
        state = State::STOPPING;

        if (requestSock)
            requestSock.shutdown(SHUT_RD);
        if (requestReader.joinable())
            requestReader.join();
    }

    bool ensureConnected() override {
        return true;
    }

public:
    /**
     * Constructs server-side.
     *
     * @param[in] node   Publisher's P2P node
     * @param[in] socks  Sockets connected to the remote peer
     */
    PubImpl(PubP2pNode& node, TcpSock socks[3])
        : Impl(node)
    {
        assignSrvrSocks(socks);
        rmtSockAddr = noticeSock.getRmtAddr();
    }

    bool rmtIsPubPath() const noexcept override {
        return true; // Because connected to me
    }
};

PubPeer::PubPeer(PubP2pNode& node, TcpSock socks[3])
    : Peer(new PubImpl(node, socks))
{}

/******************************************************************************/

/**
 * Subscriber's peer implementation. May be server-side or client-side
 * constructed.
 */
class SubImpl : public Peer::Impl
{
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

        void missed(SubP2pNode& node, Peer peer) const {
            if (id == Id::PROD_INFO) {
                node.missed(prodIndex, peer);
            }
            else if (id == Id::DATA_SEG) {
                node.missed(dataSegId, peer);
            }
            else {
                throw LOGIC_ERROR("Invalid request ID: " +
                        std::to_string((int)id));
            }
        }

        inline bool matches(const ProdInfo& prodInfo) const noexcept {
            return id == Id::PROD_INFO && prodIndex == prodInfo.getIndex();
        }

        inline bool matches(const DataSeg& dataSeg) const noexcept {
            return id == Id::DATA_SEG && dataSegId == dataSeg.getId();
        }

        bool write(TcpSock& sock) const {
            bool success;

            if (id == Id::PROD_INFO) {
                success = sock.write(
                        static_cast<PduType>(PduId::PROD_INFO_REQUEST)) &&
                        sock.write((ProdIndex::Type)prodIndex);
            }
            else if (id == Id::DATA_SEG) {
                success = sock.write(
                        static_cast<PduType>(PduId::DATA_SEG_REQUEST)) &&
                        sock.write((ProdIndex::Type)dataSegId.prodIndex) &&
                        sock.write(dataSegId.offset);
            }
            else {
                throw LOGIC_ERROR("Request is unset");
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

    SubP2pNode&        node;        // Subscriber's P2P node
    RequestQ           requests;    // Requests to be sent to remote peer
    RequestQ           requested;   // Requests sent to remote peer
    std::atomic<bool>  clientSide;  // Instance was constructed client-side?

    /**
     * Pushes a request onto the request queue.
     *
     * @tparam    ID          Type of `id`
     * @param[in] id          Identifier of request
     * @retval    `false`     No connection. Connection was lost or `start()`
     *                        wasn't called.
     * @retval    `true`      Success
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
     * Sends requests to the remote peer. Reads from the request queue and
     * writes to the request socket.
     *
     * @param[in] peer  Associated local peer
     */
    void runRequester(Peer peer) {
        try {
            for (;;) {
                auto request = requests.waitGet();

                if (!request) {
                    // `requests.stop()` called
                    requested.drainTo(node, peer);
                    requests.drainTo(node, peer);
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

    bool rcvPubPathNotice(Peer peer) {
        bool notice;
        if (read(noticeSock, notice)) {
            node.recvNotice(PubPath(notice), peer);
            rmtPubPath = notice;
            return true;
        }
        return false;
    }

    /**
     * Receives a notice about available data.
     *
     * @tparam    TYPE     Type of notice
     * @param[in] peer     Associated local peer
     * @retval    `false`  Connection lost
     * @retval    `true`   Success
     */
    template<class TYPE>
    bool rcvNotice(Peer peer) {
        TYPE notice;
        if (!read(noticeSock, notice))
            return false;
        if (!node.recvNotice(notice, peer))
            return true;
        return request(notice);
    }
    inline bool rcvProdInfoNotice(Peer peer) {
        return rcvNotice<ProdIndex>(peer);
    }
    inline bool rcvDataSegNotice(Peer peer) {
        return rcvNotice<DataSegId>(peer);
    }

    template<class DATA>
    bool rcvData(Peer peer) {
        bool success = false;
        DATA           data;
        if (read(dataSock, data)) {
            while (!requested.empty()) {
                const auto& expected = requested.front();
                if (expected.matches(data)) {
                    node.recvData(data, peer);
                    requested.pop();
                    break;
                }

                /*
                 *  NB: A missed response means that the remote peer doesn't
                 *  have the requested item.
                 */
                expected.missed(node, peer);
                requested.pop();
            }
            success = true;
        }
        return success;
    }
    inline bool rcvProdInfo(Peer peer) {
        return rcvData<ProdInfo>(peer);
    }
    inline bool rcvDataSeg(Peer peer) {
        return rcvData<DataSeg>(peer);
    }

protected:
    std::atomic<bool>  rmtPubPath;

    bool ensureConnected() override {
        if (!clientSide)
            return true; // Server-side constructed => already connected

        bool        success = false;
        TcpClntSock socks[3]; // Use of RAII sockets => sockets close on error

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
                    success     = true;
                }
            }
        }

        return success;
    }

    bool exchangeNodeTypes() override {
        bool          success = false; // Failure default
        P2pNode::Type nodeType;

        if (clientSide) {
            // Keep consonant with `PubImpl::exchangeNodeTypes()`
            if (read(noticeSock, nodeType)) {
                rmtNodeType = nodeType;
                // NB: Publishers don't care about the type of the remote node
                success = (nodeType == P2pNode::Type::PUBLISHER) ||
                        write(noticeSock, P2pNode::Type::SUBSCRIBER);
            }
        }
        else {
            if (write(noticeSock, P2pNode::Type::SUBSCRIBER) &&
                    read(noticeSock, nodeType)) {
                // NB: A publisher's local peer is never client-side constructed
                rmtNodeType = nodeType; // Will be `P2pNode::Type::SUBSCRIBER`
                success = true;
            }
        }

        return success;
    }

    void startThreads(Peer peer) override {
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
     * Dispatch function for processing an incoming message from the remote
     * peer.
     *
     * @param[in] id            Message type
     * @param[in] peer          Associated local peer
     * @retval    `false`       Connection lost
     * @retval    `true`        Success
     * @throw std::logic_error  `id` is unknown
     */
    bool processPdu(const PduId id, Peer peer) override {
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
     * Idempotent.
     */
    void stopAndJoinThreads() override {
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
    /**
     * Client-side construction.
     *
     * @param[in] node      Subscriber's P2P node
     * @param[in] srvrAddr  Socket address of remote peer-server
     */
    SubImpl(SubP2pNode& node, SockAddr srvrAddr)
        : Impl(node)
        , node(node)
        , requests()
        , requested()
        , rmtPubPath(false)
        , clientSide(true)
    {
        rmtSockAddr = srvrAddr;
    }

    /**
     * Server-side construction.
     *
     * @param[in] node   Subscriber's P2P node
     * @param[in] socks  Sockets connected to the remote peer
     */
    SubImpl(SubP2pNode& node, TcpSock socks[3])
        : Impl(node)
        , node(node)
        , requests()
        , requested()
        , rmtPubPath(false)
        , clientSide(false)
    {
        assignSrvrSocks(socks);
        rmtSockAddr = noticeSock.getRmtAddr();
    }

    bool rmtIsPubPath() const noexcept override {
        return rmtPubPath;
    }
};

SubPeer::SubPeer(SubP2pNode& node, TcpSock socks[3])
    : Peer(new SubImpl(node, socks))
{}

SubPeer::SubPeer(SubP2pNode& node, const SockAddr& srvrAddr)
    : Peer(new SubImpl(node, srvrAddr))
{}

/******************************************************************************/

/**
 * Peer-server implementation.
 *
 * @tparam NODE  Type of P2P node
 * @tparam PEER  Type of associated peer
 */
template<class NODE, class PEER>
class PeerSrvr<NODE, PEER>::Impl
{
    using PeerQ = std::queue<PEER, std::list<PEER>>;

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
    const TcpSrvrSock srvrSock;
    PeerQ             acceptQ;
    size_t            maxAccept;

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
                PEER peer{};
                if (peerFactory.add(sock, noticePort, peer))
                    acceptQ.push(peer);
                cond.notify_one();
            }
        }
    }

public:
    /**
     * Constructs from the local address for the server.
     *
     * @param[in] node       P2P node
     * @param[in] srvrAddr   Local Address for P2P server
     * @param[in] maxAccept  Maximum number of outstanding P2P connections
     */
    Impl(   NODE&           node,
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
     * Returns the next, accepted peer.
     *
     * @return Next peer
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
PeerSrvr<PubP2pNode,PubPeer>::PeerSrvr(PubP2pNode&     node,
                                       const SockAddr& srvrAddr,
                                       unsigned        maxAccept)
    : pImpl(new Impl(node, srvrAddr, maxAccept))
{}

template<>
PubPeer PeerSrvr<PubP2pNode,PubPeer>::accept() {
    return pImpl->accept();
}

template<>
PeerSrvr<SubP2pNode,SubPeer>::PeerSrvr(SubP2pNode&     node,
                                       const SockAddr& srvrAddr,
                                       unsigned        maxAccept)
    : pImpl(new Impl(node, srvrAddr, maxAccept))
{}

template<>
SubPeer PeerSrvr<SubP2pNode,SubPeer>::accept() {
    return pImpl->accept();
}

} // namespace
