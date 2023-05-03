/**
 * This file implements the Peer class. The Peer class handles low-level, bidirectional messaging
 * with its remote counterpart.
 *
 *  @file:  Peer.cpp
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */
#include "config.h"

#include "error.h"
#include "Peer.h"
#include "ThreadException.h"

#include <atomic>
#include <queue>
#include <semaphore.h>
#include <unordered_map>

namespace hycast {

/**
 * Abstract base implementation of the `Peer` interface.
 */
class PeerImpl : public Peer
{
    using NoticeQ = std::queue<Notice>;

    enum class State {
        INIT,
        STARTED,
        STOPPED
    }                  state;         ///< State of this instance
    mutable Mutex      stateMutex;    ///< Protects state changes
    mutable Mutex      noticeMutex;   ///< For accessing notice queue
    mutable Cond       noticeCond;    ///< For accessing notice queue
    Thread             noticeThread;  ///< For sending notices
    Thread             connThread;     ///< Thread for running the RPC instance
    std::once_flag     backlogFlag;   ///< Ensures that a backlog is sent only once
    P2pMgr&            p2pMgr;        ///< Associated P2P manager
    NoticeQ            noticeQ;       ///< Notice queue
    ThreadEx           threadEx;      ///< Internal thread exception
    SockAddr           lclSockAddr;   ///< Local (notice) socket address
    P2pSrvrInfo        rmtSrvrInfo;   ///< Information on the associated remote P2P server

    void setThreadEx(const std::exception& ex) {
        threadEx.set(ex);
        ::sem_post(&stopSem);
    }

    /**
     * Handles the backlog of products missing from the remote peer.
     *
     * @pre                The current thread is detached
     * @param[in] prodIds  Identifiers of complete products that the remote per has.
     */
    void runBacklog(ProdIdSet prodIds) {
        try {
            const auto missing = p2pMgr.subtract(prodIds); // Products missing from remote peer
            const auto maxSegSize = DataSeg::getMaxSegSize();

            // The regular P2P mechanism is used to notify the remote peer of data that it's missing
            for (const auto prodId : missing) {
                notify(prodId);

                /*
                 * Getting data-product information will count against this instance iff the local
                 * P2P manager is the publisher's. This might cause this instance to be judged the
                 * worst peer and, consequently, disconnected. If the local P2P manager is a
                 * subscriber, however, then this won't have that effect. This is a good thing
                 * because it's better for the network if any backlogs are obtained from subscribers
                 * rather than the publisher, IMO.
                 */
                const auto prodInfo = p2pMgr.getDatum(prodId, rmtSockAddr);
                if (prodInfo) {
                    const auto prodSize = prodInfo.getSize();
                    for (SegSize offset = 0; offset < prodSize; offset += maxSegSize)
                        notify(DataSegId(prodId, offset));
                }
            }
        }
        catch (const std::exception& ex) {
            log_error(ex);
            setThreadEx(ex);
        }
    }

protected:
    mutable sem_t      stopSem;     ///< For async-signal-safe stopping
    PeerConn::Pimpl    peerConn;    ///< Connection with remote peer
    SockAddr           rmtSockAddr; ///< Remote socket address
    std::atomic<bool>  connected;   ///< Connected to remote peer?


    /**
     * Runs the writer of notices to the remote peer.
     */
    void runNoticeWriter() {
        LOG_TRACE("Executing notice writer");
        try {
            while (connected) {
                Notice notice;
                {
                    Lock lock{noticeMutex};
                    noticeCond.wait(lock, [&]{return !noticeQ.empty();});

                    notice = noticeQ.front();
                    noticeQ.pop();
                }

                if (notice.id == Notice::Id::DATA_SEG_ID) {
                    connected = peerConn->notify(notice.dataSegId);
                        //LOG_TRACE("Peer %s notified remote about data-segment %s",
                                //to_string().data(), notice.to_string().data());
                }
                else if (notice.id == Notice::Id::PROD_INDEX) {
                    connected = peerConn->notify(notice.prodId);
                        //LOG_TRACE("Peer %s notified remote about product %s",
                                //to_string().data(), notice.to_string().data());
                }
                else if (notice.id == Notice::Id::PEER_SRVR_INFO) {
                    connected = peerConn->add(notice.srvrInfo);
                        //LOG_TRACE("Peer %s notified remote about good P2P server %s",
                                //to_string().data(), notice.to_string().data());
                }
                else if (notice.id == Notice::Id::PEER_SRVR_INFOS) {
                    connected = peerConn->add(notice.tracker);
                        //LOG_TRACE("Peer %s notified remote about good P2P servers %s",
                                //to_string().data(), notice.to_string().data());
                }
                else {
                    throw LOGIC_ERROR("Notice ID is unknown: " + std::to_string((int)notice.id));
                }
            }

            ::sem_post(&stopSem);
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
            setThreadEx(ex);
        }
        LOG_TRACE("Terminating");
    }

    /**
     * Starts the writer of notices to the remote peer on a separate thread.
     */
    void startNoticeWriter() {
        //LOG_DEBUG("Starting notice-writer thread");
        noticeThread = Thread(&PeerImpl::runNoticeWriter, this);
    }
    /**
     * Idempotent.
     */
    void stopNoticeWriter() {
        LOG_ASSERT(!stateMutex.try_lock());
        if (noticeThread.joinable()) {
            ::pthread_cancel(noticeThread.native_handle());
            noticeThread.join();
        }
    }

    /**
     * Executes the connection.
     */
    void runConn() {
        try {
            peerConn->run();
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }
    }

    /**
     * Stops the connection. Idempotent.
     */
    void stopConn() {
        LOG_ASSERT(!stateMutex.try_lock());
        if (connThread.joinable()) {
            peerConn->halt();
            connThread.join();
        }
    }

    /**
     * @pre               The state mutex is unlocked
     * @throw LogicError  The state isn't INIT
     * @post              The state is STARTED
     * @post              The state mutex is unlocked
     */
    void startImpl() {
        Guard guard{stateMutex};
        if (state != State::INIT)
            throw LOGIC_ERROR("Instance can't be re-executed");

        noticeThread = Thread(&PeerImpl::runNoticeWriter, this);
        try {
            connThread = Thread(&PeerImpl::runConn, this);
        }
        catch (const std::exception& ex) {
            stopNoticeWriter();
            throw;
        } // Notice writer thread started

        state = State::STARTED;
    }

    /**
     * Idempotent.
     *
     * @pre               The state mutex is unlocked
     * @throw LogicError  The state is not STARTED
     * @post              The state is STOPPED
     * @post              The state mutex is unlocked
     */
    void stopImpl() {
        Guard guard{stateMutex};
        stopConn();          // Idempotent
        stopNoticeWriter(); // Idempotent
        state = State::STOPPED;
    }

    /**
     * Adds a notice to be sent.
     * @param[in] notice   The notice to be sent
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool addNotice(const Notice& notice) {
        threadEx.throwIfSet();

        if (connected && rmtSrvrInfo.tier != 0) { // Publisher's don't receive notices
            //LOG_TRACE("Peer %s is adding notice %s", to_string().data(),
                    //notice.to_string().data());
            Guard guard{noticeMutex};
            noticeQ.push(notice);
            noticeCond.notify_all();
        }

        return connected;
    }

public:
    /**
     * Constructs.
     *
     * @param[in] p2pMgr          P2P manager
     * @param[in] conn            Connection with remote peer
     */
    PeerImpl(
            P2pMgr&         p2pMgr,
            PeerConn::Pimpl conn)
        : state(State::INIT)
        , stateMutex()
        , noticeMutex()
        , noticeCond()
        , noticeThread()
        , connThread()
        , backlogFlag()
        , p2pMgr(p2pMgr)
        , noticeQ()
        , threadEx()
        , lclSockAddr(conn->getLclAddr())
        , rmtSrvrInfo()
        , stopSem()
        , peerConn(conn)
        , rmtSockAddr(conn->getRmtAddr())
        , connected{true}
    {
        if (::sem_init(&stopSem, 0, 0) == -1)
            throw SYSTEM_ERROR("Couldn't initialize semaphore");
    }

    /**
     * Copy constructs. Deleted because of "rule of three".
     * @param[in] impl  Instance to copy
     */
    PeerImpl(const PeerImpl& impl) =delete; // Rule of three

    /**
     * Destroys.
     */
    virtual ~PeerImpl() noexcept {
        Guard guard{stateMutex};
        LOG_ASSERT(state == State::INIT || state == State::STOPPED);
        ::sem_destroy(&stopSem);
    }

    /**
     * Assigns. Deleted because of "rule of three".
     * @param[in] rhs  Right hand side of assignment
     * @return Reference to assigned value
     */
    PeerImpl& operator=(const PeerImpl& rhs) noexcept =delete; // Rule of three

    /**
     * Returns the socket address of the local peer.
     *
     * @return Socket address of local peer
     */
    SockAddr getLclAddr() const noexcept override {
        return lclSockAddr;
    }

    /**
     * Returns the socket address of the remote peer.
     *
     * @return Socket address of remote peer
     */
    SockAddr getRmtAddr() const noexcept override {
        return rmtSockAddr;
    }

    virtual bool isClient() const noexcept;

    virtual bool isRmtPub() const noexcept;

    /**
     * Returns the hash code of this instance.
     * @return The hash code of this instance
     */
    size_t hash() const noexcept override {
        // Keep consistent with `operator<()`
        return rmtSockAddr.hash() ^ lclSockAddr.hash();
    }

    /**
     * Indicates if this instance is less than another.
     * @param[in] rhs      The right-hand-side other instance
     * @retval    true     This instance is less than the other
     * @retval    false    This instance is not less than the other
     */
    bool operator<(const Peer& rhs) const noexcept override {
        // Keep consistent with `hash()`
        return rmtSockAddr < rhs.getRmtAddr()
                ? true
                : rhs.getRmtAddr() < rmtSockAddr
                      ? false
                      : lclSockAddr < rhs.getLclAddr();
    }

    /**
     * Indicates if this instance is not equal to another.
     * @param[in] rhs      The right-hand-side other instance
     * @retval    true     This instance is not equal to the other
     * @retval    false    This instance is equal to the other
     */
    bool operator!=(const Peer& rhs) const noexcept override {
        return *this < rhs || rhs < *this;
    }

    /**
     * Indicates if this instance is equal to another.
     * @param[in] rhs      The right-hand-side other instance
     * @retval    true     This instance is equal to the other
     * @retval    false    This instance is not equal to the other
     */
    bool operator==(const Peer& rhs) const noexcept override {
        return !(*this == rhs);
    }

    /**
     * Returns a string representation of this instance.
     *
     * @return  String representation of this instance
     */
    String to_string() const override {
        return "{lcl=" + lclSockAddr.to_string() + ", rmt=" + rmtSockAddr.to_string() + "}";
    }

    void run() override {
        startImpl();
        /*
         * Blocks until
         *   - `connected` is false
         *   - `halt()` called
         *   - `threadEx` is true
         */
        ::sem_wait(&stopSem);
        stopImpl();
        threadEx.throwIfSet();
    }

    void halt() override {
        int semval = 0;
        ::sem_getvalue(&stopSem, &semval);
        if (semval < 1)
            ::sem_post(&stopSem);
    }

    void notifyHavePubPath(const bool havePubPath) override {
        addNotice(Notice{havePubPath});
    }

    void add(const P2pSrvrInfo& srvrInfo) override {
        addNotice(Notice{srvrInfo});
    }
    void add(const Tracker& tracker) override {
        addNotice(Notice{tracker});
    }

    void notify(const ProdId prodId) override {
        addNotice(Notice{prodId});
    }
    /**
     * Notifies the remote counterpart about an available data segment.
     * @param[in] segId  ID of the data segment
     */
    void notify(const DataSegId segId) override {
        addNotice(Notice{segId});
    }

    void recvHaveProds(ProdIdSet prodIds) override {
        std:call_once(backlogFlag, [&]{
                Thread(&PeerImpl::runBacklog, this, prodIds).detach();});
    }

    /**
     * Processes a request for product information.
     *
     * @param[in] prodId     Index of the product
     */
    void recvRequest(const ProdId prodId) override {
        //LOG_TRACE("Peer %s received request for information on product %s", to_string().data(),
         //       prodId.to_string().data());
        auto prodInfo = p2pMgr.getDatum(prodId, rmtSockAddr);
        if (prodInfo) {
            //LOG_TRACE("Peer %s is sending information on product %s", to_string().data(),
             //       prodId.to_string().data());
            if (!(connected = peerConn->send(prodInfo)))
                ::sem_post(&stopSem);
        }
    }

    /**
     * Processes a request for a data segment.
     *
     * @param[in] dataSegId     Data segment ID
     */
    void recvRequest(const DataSegId dataSegId) override {
        //LOG_TRACE("Peer %s received request for data segment %s", to_string().data(),
                //dataSegId.to_string().data());
        auto dataSeg = p2pMgr.getDatum(dataSegId, rmtSockAddr);
        if (dataSeg) {
            //LOG_TRACE("Peer %s is sending data segment %s", to_string().data(),
                    //dataSegId.to_string().data());
            if (!(connected = peerConn->send(dataSeg)))
                ::sem_post(&stopSem);
        }
    }
};

/******************************************************************************/

/**
 * Publisher's peer implementation. The peer will be constructed server-side.
 */
class PubPeerImpl final : public PeerImpl
{
public:
    /**
     * Constructs.
     *
     * @param[in] p2pMgr        Publisher's P2P manager
     * @param[in] conn          Connection with remote peer
     * @throw     RuntimeError  Lost connection
     */
    PubPeerImpl(
            PubP2pMgr&      p2pMgr,
            PeerConn::Pimpl conn)
        : PeerImpl(p2pMgr, conn)
    {}

    ~PubPeerImpl() noexcept {
        try {
            stopImpl(); // Idempotent
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }
    }

    bool isRmtPub() const noexcept override {
        return false; // Can't be unless I'm connected to myself, which shouldn't happen
    }

    bool isClient() const noexcept override {
        return false;
    }

    void request(const ProdId prodId) override {
        throw LOGIC_ERROR("Shouldn't have been called");
    }
    void request(const DataSegId& segId) override {
        throw LOGIC_ERROR("Shouldn't have been called");
    }

    void recvAdd(const P2pSrvrInfo& p2pSrvr) override {
        throw LOGIC_ERROR("Shouldn't have been called");
    }
    void recvAdd(const Tracker tracker) override {
        throw LOGIC_ERROR("Shouldn't have been called");
    }

    bool recvNotice(const ProdId prodId) override {
        throw LOGIC_ERROR("Shouldn't have been called");
        return false;
    }
    bool recvNotice(const DataSegId dataSegId) override {
        throw LOGIC_ERROR("Shouldn't have been called");
        return false;
    }

    void recvData(const ProdInfo prodInfo) override {
        throw LOGIC_ERROR("Shouldn't have been called");
    }
    void recvData(const DataSeg dataSeg) override {
        throw LOGIC_ERROR("Shouldn't have been called");
    }

    void drainPending() override {
        throw LOGIC_ERROR("Shouldn't have been called");
    }
};

Peer::Pimpl Peer::create(
        PubP2pMgr&      p2pMgr,
        PeerConn::Pimpl conn) {
    return Pimpl(new PubPeerImpl(p2pMgr, conn));
}

/**************************************************************************************************/

/**
 * Subscriber's peer implementation. May be constructed server-side or client-side.
 */
class SubPeerImpl final : public PeerImpl
{
    /**
     * A thread-safe, linked-map of datum requests.
     */
    class RequestQ {
        struct HashRequest
        {
            size_t operator()(const Notice& request) const noexcept {
                return request.hash();
            }
        };

        /// The value component references the next entry in a linked list
        using Requests = std::unordered_map<Notice, Notice, HashRequest>;

        mutable Mutex mutex;
        mutable Cond  cond;
        Requests      requests;
        Notice        head;
        Notice        tail;
        bool          stopped;

        /**
         * Deletes the request at the head of the queue.
         *
         * @pre    Mutex is locked
         * @pre    Queue is not empty
         * @return The former head of the queue
         * @post   Mutex is locked
         */
        Notice deleteHead() {
            auto oldHead = head;
            auto newHead = requests[head];
            requests.erase(head);
            head = newHead;
            if (!head)
                tail = head; // Queue is empty
            return oldHead;
        }

    public:
        /// Iterator for a request-queue
        class Iter : public std::iterator<std::input_iterator_tag, Notice>
        {
            RequestQ& queue;
            Notice    request;

        public:
            /**
             * Constructs.
             * @param[in] queue    Queue of requests
             * @param[in] request  First request to return
             */
            Iter(RequestQ& queue, const Notice& request)
                : queue(queue)
                , request(request) {}
            /**
             * Copy constructs.
             * @param[in] iter  The other instance
             */
            Iter(const Iter& iter)
                : queue(iter.queue)
                , request(iter.request) {}
            /// Inequality operator
            bool operator!=(const Iter& rhs) const {
                return request ? (request != rhs.request) : false;
            }
            /// Dereference operator
            Notice& operator*()  {return request;}
            /// Dereference operator
            Notice* operator->() {return &request;}
            /// Increment operator
            Iter& operator++() {
                Guard guard{queue.mutex};
                request = queue.requests[request]; // Value component references next entry
                return *this;
            }
        };

        /**
         * Default constructs.
         */
        RequestQ()
            : mutex()
            , cond()
            , requests()
            , head()
            , tail()
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
        size_t push(const Notice& request) {
            Guard guard{mutex};
            requests.insert({request, Notice{}});
            if (tail)
                requests[tail] = request;
            if (!head)
                head = request;
            tail = request;
            cond.notify_all();
            return requests.size();
        }

        size_t count(const Notice& request) {
            Guard guard{mutex};
            return requests.count(request);
        }

        /**
         * Indicates if the queue is empty.
         *
         * @retval true     Queue is empty
         * @retval false    Queue is not empty
         */
        bool empty() const {
            Guard guard{mutex};
            return requests.empty();
        }

        /**
         * Deletes the request at the head of the queue.
         *
         * @throw OutOfRange  Queue is empty
         */
        void pop() {
            Guard guard{mutex};
            if (requests.empty())
                throw OUT_OF_RANGE("Queue is empty");
            deleteHead();
        }

        /**
         * Removes and returns the request at the head of the queue. Blocks until it exists or
         * `stop()` is called.
         *
         * @return  Head request or false one if `stop()` was called
         * @see     `stop()`
         */
        Notice waitGet() {
            Lock lock{mutex};
            cond.wait(lock, [&]{return !requests.empty() || stopped;});
            if (stopped)
                return Notice{};
            return deleteHead();
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
         * Tells a subscriber's P2P manager about all requests in the queue in the order in which
         * they were added, then clears the queue.
         *
         * @param[in] p2pMgr       Subscriber's P2P manager
         * @param[in] rmtSockAddr  Socket address of the remote peer
         * @see SubP2pNode::missed(ProdId, SockAddr)
         * @see SubP2pNode::missed(DataSegId, SockAddr)
         */
        void drainTo(
                SubP2pMgr&      p2pMgr,
                const SockAddr rmtSockAddr) {
            Guard guard{mutex};
            while (head) {
                if (head.id == Notice::Id::PROD_INDEX) {
                    p2pMgr.missed(head.prodId, rmtSockAddr);
                }
                else if (head.id == Notice::Id::DATA_SEG_ID) {
                    p2pMgr.missed(head.dataSegId, rmtSockAddr);
                }
                head = requests[head];
            }
            requests.clear();
            tail = head;
        }

        Iter begin() {
            Guard guard{mutex};
            return Iter(*this, head);
        }

        Iter end() {
            Guard guard{mutex};
            return Iter{*this, Notice{}};
        }
    }; // RequestQ

    SubP2pMgr&     subP2pMgr;   ///< Subscriber's P2P manager
    RequestQ       requested;   ///< Requests sent to remote peer
    std::once_flag backlogFlag; ///< Ensures that the backlog is requested only once
    P2pSrvrInfo    rmtSrvrInfo; ///< Information on the remote P2P server

    /**
     * Requests the backlog of data that's already been transmitted but that this instance doesn't
     * have.
     */
    void requestBacklog() {
        const auto prodIds = subP2pMgr.getProdIds(); // All complete products
        if (prodIds.size())
            /**
             * Informing the remote peer of data this instance doesn't know about is impossible;
             * therefore, the remote peer is informed of data that this instance has, which will
             * cause the remote peer to notify this instance of data that it has but that this
             * instance doesn't.
             */
            if (!(connected = peerConn->request(prodIds)))
                ::sem_post(&stopSem);
    }

    /**
     * Received a datum from the remote peer. If the datum wasn't requested, then an exception is
     * thrown. Each request in the requested queue before the request for the given datum is removed
     * from the queue and passed to the P2P manager as not satisfiable by the remote peer.
     * Otherwise, the datum is passed to the subscriber's P2P manager and the pending request is
     * removed from the requested queue.
     *
     * @tparam    DATUM       Type of datum: `ProdInfo` or `DataSeg`
     * @param[in] datum       Datum
     * @throw     LogicError  Datum wasn't requested
     * @see `SubP2pMgr::missed()`
     */
    template<typename DATUM>
    void processData(const DATUM datum) {
        //LOG_DEBUG("Processing datum %s", datum.to_string().data());
        const auto& id = datum.getId();
        if (requested.count(Notice{id}) == 0)
            throw LOGIC_ERROR("Peer " + to_string() + " received unrequested datum " +
                    datum.to_string());

        for (auto iter =  requested.begin(), end = requested.end(); iter != end; ) {
            //LOG_DEBUG("Comparing against request %s", iter->to_string().data());
            if (iter->equals(id)) {
                subP2pMgr.recvData(datum, rmtSockAddr);
                requested.pop();
                return;
            }
            /*
             * NB: A skipped response from the remote peer means that it doesn't have the requested
             * data.
             */
            if (iter->getType() == Notice::Id::DATA_SEG_ID) {
                subP2pMgr.missed(iter->dataSegId, rmtSockAddr);
            }
            else if (iter->getType() == Notice::Id::PROD_INDEX) {
                subP2pMgr.missed(iter->prodId, rmtSockAddr);
            }

            ++iter; // Must occur before `requested.pop()`
            requested.pop();
        }
        throw LOGIC_ERROR("Peer " + to_string() + " received unrequested datum " +
                datum.to_string());
    }

public:
    /**
     * Constructs -- both client- and server-side.
     *
     * @param[in] p2pMgr        Subscriber's P2P manager
     * @param[in] conn          Pointer to peer-connection
     * @throw     LogicError    Destination port number is zero
     * @throw     SystemError   Couldn't connect. Bad failure.
     * @throw     RuntimeError  Couldn't connect. Might be temporary.
     */
    SubPeerImpl(
            SubP2pMgr&      p2pMgr,
            PeerConn::Pimpl conn)
        : PeerImpl(p2pMgr, conn)
        , subP2pMgr(p2pMgr)
        , requested()
        , backlogFlag()
    {
        // TODO: Enable to allow sending local server's address to remote?
        // if (!rpc->sendSrvrAddr(subP2pMgr.getSrvrAddr()))
            // throw RUNTIME_ERROR("rpc::sendSrvrAddr() failure");
    }

    ~SubPeerImpl() noexcept {
        try {
            stopImpl();
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }
    }

    bool isClient() const noexcept override {
        return peerConn->isClient();
    }

    bool isRmtPub() const noexcept override {
        return rmtSrvrInfo.tier == 0;
    }

    void request(const ProdId prodId) override {
        requested.push(Notice{prodId});
        if ((connected = peerConn->request(prodId))) {
            //LOG_TRACE("Peer %s requested information on product %s", to_string().data(),
                    //prodId.to_string().data());
        }
        else {
            ::sem_post(&stopSem);
        }
    }
    void request(const DataSegId& segId) override {
        requested.push(Notice{segId});
        if ((connected = peerConn->request(segId))) {
            //LOG_TRACE("Peer %s requested information on data-segment %s", to_string().data(),
                    //segId.to_string().data());
        }
        else {
            ::sem_post(&stopSem);
        }
    }

    void recvAdd(const P2pSrvrInfo& srvrInfo) override {
        subP2pMgr.recvAdd(srvrInfo);
    }
    void recvAdd(const Tracker tracker) override {
        subP2pMgr.recvAdd(tracker);
    }

    /**
     * Receives a notice about available product information. Notifies the subscriber's P2P manager.
     * Requests the datum if told to do so by the P2P manager.
     *
     * The first time this function is called, it tells the remote peer what complete data-products
     * it has.
     *
     * @param[in] prodId     Product index
     * @retval    false      Connection lost
     * @retval    true       Success
     */
    bool recvNotice(const ProdId prodId) {
        //LOG_TRACE("Peer %s received notice about product %s", to_string().data(),
                //prodId.to_string().data());

        std::call_once(backlogFlag, [&]{requestBacklog();});

        if (subP2pMgr.recvNotice(prodId, rmtSockAddr)) {
            // Subscriber wants the product information
            request(prodId);
        }
        else {
            LOG_TRACE("Peer %s didn't request information on product %s", to_string().data(),
                    prodId.to_string().data());
        }
        return connected;
    }
    /**
     * Receives a notice about an available data segment. Notifies the subscriber's P2P manager.
     * Requests the datum if told to do so by the P2P manager.
     *
     * The first time this function is called, it tells the remote peer what complete data-products
     * it has.
     *
     * @param[in] dataSegId  Data segment ID
     * @retval    false      Connection lost
     * @retval    true       Success
     */
    bool recvNotice(const DataSegId dataSegId) {
        //LOG_TRACE("Peer %s received notice about data segment %s", to_string().data(),
                //dataSegId.to_string().data());

        std::call_once(backlogFlag, [&]{requestBacklog();});

        if (subP2pMgr.recvNotice(dataSegId, rmtSockAddr)) {
            // Subscriber wants the data segment
            request(dataSegId);
        }
        else {
            LOG_TRACE("Peer %s didn't request data segment %s", to_string().data(),
                    dataSegId.to_string().data());
        }
        return connected;
    }

    /**
     * Receives product information from the remote peer.
     *
     * @param[in] prodInfo  Product information
     */
    void recvData(const ProdInfo prodInfo) {
        //LOG_TRACE("Peer %s received information on product %s",
                //to_string().data(), prodInfo.getId().to_string().data());
        processData<ProdInfo>(prodInfo);
    }
    /**
     * Receives a data segment from the remote peer.
     *
     * @param[in] dataSeg  Data segment
     */
    void recvData(const DataSeg dataSeg) {
        //LOG_TRACE("Peer %s received data-segment %s", to_string().data(),
                //dataSeg.to_string().data());
        processData<DataSeg>(dataSeg);
    }

    void drainPending() override {
        requested.drainTo(subP2pMgr, rmtSockAddr);
    }
};

Peer::Pimpl Peer::create(
        SubP2pMgr&     p2pMgr,
        const SockAddr srvrAddr) {
    auto conn = PeerConn::create(srvrAddr);
    auto pImpl = Pimpl{new SubPeerImpl{p2pMgr, conn}};
    conn->setPeer(pImpl);
    return pImpl;
}

/******************************************************************************/

/// An implementation of a P2P-server
template<typename P2P_MGR>
class PeerSrvrImpl : public P2pSrvr<P2P_MGR>
{
    PeerConnSrvr::Pimpl peerConnSrvr;

public:
    /// Constructs
    PeerSrvrImpl(
            const SockAddr srvrAddr,
            const unsigned maxPendConn)
        : peerConnSrvr(PeerConnSrvr::create(srvrAddr, maxPendConn))
    {}

    /// Constructs
    PeerSrvrImpl(PeerConnSrvr::Pimpl peerConnSrvr)
        : peerConnSrvr(peerConnSrvr)
    {}

    SockAddr getSrvrAddr() const override {
        return peerConnSrvr->getSrvrAddr();
    }

    Peer::Pimpl accept(P2P_MGR& p2pMgr) override {
        auto peerConn = peerConnSrvr->accept();
        return peerConn
                ? Peer::create(p2pMgr, peerConn)
                : Peer::Pimpl{};
    }

    /**
     * Causes `accept()` to return a false object.
     * @see accept()
     */
    void halt() override {
        peerConnSrvr->halt();
    }
};

template<>
P2pSrvr<PubP2pMgr>::Pimpl P2pSrvr<PubP2pMgr>::create(
        const SockAddr  srvrAddr,
        const unsigned  maxPendConn) {
    return Pimpl{new PeerSrvrImpl<PubP2pMgr>(srvrAddr, maxPendConn)};
}

/**
 * Returns a smart pointer to an implementation.
 * @param[in] srvrAddr     Socket address to be used by the server. Must not be wildcard. A port
 *                         number of zero obtains a system chosen one.
 * @param[in] maxPendConn  Maximum number of pending connections
 */
template<>
P2pSrvr<SubP2pMgr>::Pimpl P2pSrvr<SubP2pMgr>::create(
        const SockAddr srvrAddr,
        const unsigned maxPendConn) {
    return Pimpl{new PeerSrvrImpl<SubP2pMgr>(srvrAddr, maxPendConn)};
}

template<>
P2pSrvr<SubP2pMgr>::Pimpl P2pSrvr<SubP2pMgr>::create(const PeerConnSrvr::Pimpl peerConnSrvr) {
    return Pimpl{new PeerSrvrImpl<SubP2pMgr>(peerConnSrvr)};
}

} // namespace
