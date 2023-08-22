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
class BasePeerImpl : public Peer
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
    Thread             connThread;    ///< Thread for running the peer-connection
    std::once_flag     backlogFlag;   ///< Ensures that a backlog is sent only once
    BaseMgr&           basePeerMgr;   ///< Associated peer manager
    NoticeQ            noticeQ;       ///< Notice queue
    ThreadEx           threadEx;      ///< Internal thread exception
    SockAddr           lclSockAddr;   ///< Local (notice) socket address

    /**
     * Exchanges information on the local and remote P2P servers.
     * @param[in] lclSrvrInfo  Information on the local P2P server
     * @retval true            Success
     * @retval false           Connection lost
     */
    bool xchgSrvrInfo(const P2pSrvrInfo& lclSrvrInfo) {
        return peerConnPtr->isClient()
                // By convention, client-side peers send then receive
                ? peerConnPtr->send(lclSrvrInfo) && peerConnPtr->recv(rmtSrvrInfo)
                // And server-side peers receive then send
                : peerConnPtr->recv(rmtSrvrInfo) && peerConnPtr->send(lclSrvrInfo);
    }

    void setThreadEx() {
        threadEx.set();
        ::sem_post(&stopSem);
    }

    /**
     * Handles the backlog of products missing from the remote peer. Meant to be the start-routine
     * of a separate thread. Calls `setThreadEx()` on error.
     *
     * @param[in] prodIds  Identifiers of complete products that the remote per has
     * @see notify()
     * @see setThreadEx()
     */
    void runBacklog(ProdIdSet prodIds) {
        try {
            const auto missing = basePeerMgr.subtract(prodIds); // Products missing from remote peer
            const auto maxSegSize = DataSeg::getMaxSegSize();

            // The regular P2P mechanism is used to notify the remote peer of data that it's missing
            for (const auto prodId : missing) {
                notify(prodId);

                /*
                 * Making requests of the peer manager will count against this peer iff the peer
                 * manager is the publisher's. This might cause this peer to be judged the worst
                 * peer and, consequently, disconnected. If the local peer manager is a subscriber,
                 * however, then this won't have that effect. This is a good thing because it's
                 * better for the network if any backlogs are obtained from subscribers rather than
                 * from the publisher, IMO.
                 */
                const auto prodInfo = basePeerMgr.getDatum(prodId, rmtSockAddr);
                if (prodInfo) {
                    const auto prodSize = prodInfo.getSize();
                    for (SegSize offset = 0; offset < prodSize; offset += maxSegSize)
                        notify(DataSegId(prodId, offset));
                }
            }
        }
        catch (const std::exception& ex) {
            log_error(ex);
            setThreadEx();
        }
    }

protected:
    mutable sem_t      stopSem;     ///< For async-signal-safe stopping
    PeerConnPtr        peerConnPtr; ///< Connection with remote peer
    SockAddr           rmtSockAddr; ///< Remote socket address
    std::atomic<bool>  connected;   ///< Connected to remote peer?
    P2pSrvrInfo        rmtSrvrInfo; ///< Information on the remote peer's P2P-server

    /// Sets the disconnected flag
    void setDisconnected() noexcept
    {
        connected = false;
        ::sem_post(&stopSem);
    }

    /// Sets the exception thrown on an internal thread to the current exception.
    void setException()
    {
        threadEx.set();
        ::sem_post(&stopSem);
    }

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
                    connected = peerConnPtr->notify(notice.dataSegId);
                        //LOG_TRACE("Peer %s notified remote about data-segment %s",
                                //to_string().data(), notice.to_string().data());
                }
                else if (notice.id == Notice::Id::PROD_INDEX) {
                    connected = peerConnPtr->notify(notice.prodId);
                        //LOG_TRACE("Peer %s notified remote about product %s",
                                //to_string().data(), notice.to_string().data());
                }
                else if (notice.id == Notice::Id::P2P_SRVR_INFO) {
                    connected = peerConnPtr->notify(notice.srvrInfo);
                        //LOG_TRACE("Peer %s notified remote about good P2P servers %s",
                                //to_string().data(), notice.to_string().data());
                }
                else {
                    throw LOGIC_ERROR("Notice ID is unknown: " + std::to_string((int)notice.id));
                }
            }

            setDisconnected();
        }
        catch (const std::exception& ex) {
            setThreadEx();
        }
        LOG_TRACE("Terminating");
    }

    /**
     * Starts the writer of notices to the remote peer on a separate thread.
     */
    void startNoticeWriter() {
        //LOG_DEBUG("Starting notice-writer thread");
        noticeThread = Thread(&BasePeerImpl::runNoticeWriter, this);
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
            peerConnPtr->run(*this);
            setDisconnected();
        }
        catch (const std::exception& ex) {
            setException();
        }
    }

    /**
     * Stops the connection. Idempotent.
     */
    void stopConn() {
        LOG_ASSERT(!stateMutex.try_lock());
        if (connThread.joinable()) {
            peerConnPtr->halt();
            connThread.join();
        }
    }

    /**
     * Executes this instance. Starts sending to and receiving from the remote peer. This function
     * is the default implementation.
     *
     * @pre               The state mutex is unlocked
     * @throw LogicError  The state isn't INIT
     * @post              The state is STARTED
     * @post              The state mutex is unlocked
     */
    virtual void startImpl() {
        Guard guard{stateMutex};
        if (state != State::INIT)
            throw LOGIC_ERROR("Instance can't be re-executed");

        noticeThread = Thread(&BasePeerImpl::runNoticeWriter, this);
        try {
            connThread = Thread(&BasePeerImpl::runConn, this);
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

        if (connected && !isRmtPub()) { // Publisher's don't receive notices
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
     * @param[in] baseMgr   Base peer manager
     * @param[in] conn      Connection with remote peer
     * @throw SystemError   System failure
     * @throw RuntimeError  Couldn't exchange server information with remote peer
     */
    BasePeerImpl(
            BaseMgr&     baseMgr,
            PeerConnPtr  conn)
        : state(State::INIT)
        , stateMutex()
        , noticeMutex()
        , noticeCond()
        , noticeThread()
        , connThread()
        , backlogFlag()
        , basePeerMgr(baseMgr)
        , noticeQ()
        , threadEx()
        , lclSockAddr(conn->getLclAddr())
        , stopSem()
        , peerConnPtr(conn)
        , rmtSockAddr(conn->getRmtAddr())
        , connected{true}
        , rmtSrvrInfo()
    {
        LOG_DEBUG("Constructing peer " + to_string());

        if (::sem_init(&stopSem, 0, 0) == -1)
            throw SYSTEM_ERROR("Couldn't initialize semaphore");

        LOG_DEBUG("Exchanging P2P server information");
        if (!xchgSrvrInfo(baseMgr.getSrvrInfo()))
            throw RUNTIME_ERROR("Lost connection with " + rmtSockAddr.to_string());
    }

    /**
     * Copy constructs. Deleted because of "rule of three".
     * @param[in] impl  Instance to copy
     */
    BasePeerImpl(const BasePeerImpl& impl) =delete; // Rule of three

    /**
     * Destroys.
     */
    virtual ~BasePeerImpl() noexcept {
        LOG_TRACE("Destroying peer " + to_string());
        Guard guard{stateMutex};
        LOG_ASSERT(state == State::INIT || state == State::STOPPED);
        ::sem_destroy(&stopSem);
    }

    PeerConnPtr& getConnection() override {
        return peerConnPtr;
    }

    bool isClient() const noexcept override {
        return peerConnPtr->isClient();
    }

    bool isRmtPub() const noexcept override {
        return rmtSrvrInfo.tier == 0; // A publisher's peer is always tier 0
    }

    virtual P2pSrvrInfo::Tier getTier() const noexcept =0;

    /**
     * Returns the socket address of the local peer.
     *
     * @return Socket address of local peer
     */
    const SockAddr& getLclAddr() const noexcept override {
        return lclSockAddr;
    }

    /**
     * Returns the socket address of the remote peer.
     *
     * @return Socket address of remote peer
     */
    const SockAddr& getRmtAddr() const noexcept override {
        return rmtSockAddr;
    }

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
     * Assigns. Deleted because of "rule of three".
     * @param[in] rhs  Right hand side of assignment
     * @return Reference to assigned value
     */
    BasePeerImpl& operator=(const BasePeerImpl& rhs) noexcept =delete; // Rule of three

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
        LOG_TRACE("connected=" + std::to_string(connected) + "; threadEx=" +
                (threadEx ? "true" : "false"));
        stopImpl();
        threadEx.throwIfSet();
    }

    void halt() override {
        LOG_TRACE("Called");
        int semval = 0;
        ::sem_getvalue(&stopSem, &semval);
        if (semval < 1)
            ::sem_post(&stopSem);
    }

    void recv(const P2pSrvrInfo& srvrInfo) override {
        if (!srvrInfo)
            throw INVALID_ARGUMENT("Remote peer's P2P-server information is invalid");

        LOG_DEBUG("Peer " + to_string() + " received P2P server-info " + srvrInfo.to_string());
        Guard guard{stateMutex};
        rmtSrvrInfo = srvrInfo;
    }

    void recv(const Tracker& tracker) override {
        LOG_DEBUG("Peer " + to_string() + " received tracker " + tracker.to_string());
        basePeerMgr.recv(tracker);
    }

    P2pSrvrInfo getRmtSrvrInfo() noexcept override {
        Guard guard{stateMutex};
        return rmtSrvrInfo;
    }

    void add(const Tracker& tracker) override {
        LOG_DEBUG("Peer " + to_string() + " queuing tracker notice " + tracker.to_string());
        addNotice(Notice{tracker});
    }

    void notify(const P2pSrvrInfo& srvrInfo) override {
        LOG_DEBUG("Peer " + to_string() + " queuing P2P-server notice " + srvrInfo.to_string());
        addNotice(Notice{srvrInfo});
    }

    void notify(const ProdId prodId) override {
        LOG_DEBUG("Peer " + to_string() + " queuing product-ID notice " + prodId.to_string());
        addNotice(Notice{prodId});
    }
    /**
     * Notifies the remote peer about an available data segment.
     * @param[in] segId  ID of the data segment
     */
    void notify(const DataSegId segId) override {
        LOG_DEBUG("Peer " + to_string() + " queuing segment-ID notice " + segId.to_string());
        addNotice(Notice{segId});
    }

    void recvHaveProds(ProdIdSet prodIds) override {
        std:call_once(backlogFlag, [&]{Thread(&BasePeerImpl::runBacklog, this, prodIds).detach();});
    }

    virtual void request(const ProdId prodId) =0;

    virtual void request(const DataSegId& segId) =0;

    void recvNotice(const P2pSrvrInfo& srvrInfo) override {
        LOG_DEBUG("Peer " + to_string() + " received notice about P2P-server " + srvrInfo.to_string());
        basePeerMgr.recvNotice(srvrInfo);
    }

    virtual bool recvNotice(const ProdId prodId) =0;

    virtual bool recvNotice(const DataSegId dataSegId) =0;

    /**
     * Processes a request for product information.
     *
     * @param[in] prodId     Index of the product
     */
    void recvRequest(const ProdId prodId) override {
        LOG_DEBUG("Peer " + to_string() + " received product-info request " + prodId.to_string());
        //LOG_TRACE("Peer %s received request for information on product %s", to_string().data(),
         //       prodId.to_string().data());
        auto prodInfo = basePeerMgr.getDatum(prodId, rmtSockAddr);
        if (prodInfo) {
            LOG_DEBUG("Peer " + to_string() + " sending product-info " + prodInfo.to_string());
            connected = peerConnPtr->send(prodInfo);
            if (!connected) {
                LOG_INFO("Peer " + to_string() + " is disconnected");
                ::sem_post(&stopSem);
            }
        }
    }

    /**
     * Processes a request for a data segment.
     *
     * @param[in] dataSegId     Data segment ID
     */
    void recvRequest(const DataSegId dataSegId) override {
        LOG_DEBUG("Peer " + to_string() + " received segment request " + dataSegId.to_string());
        auto dataSeg = basePeerMgr.getDatum(dataSegId, rmtSockAddr);
        if (dataSeg) {
            LOG_DEBUG("Peer " + to_string() + " sending data-segment " + dataSegId.to_string());
            if (!(connected = peerConnPtr->send(dataSeg))) {
                LOG_INFO("Peer " + to_string() + " is disconnected");
                ::sem_post(&stopSem);
            }
        }
    }

    virtual void recvData(const ProdInfo prodInfo) =0;

    virtual void recvData(const DataSeg dataSeg) =0;

    virtual void drainPending() =0;
};

/******************************************************************************/

/**
 * Publisher's peer implementation. The peer will be constructed server-side.
 */
class PubPeerImpl final : public BasePeerImpl
{
public:
    /**
     * Constructs.
     *
     * @param[in] pubMgr           Publisher's Peer manager
     * @param[in] conn             Connection with remote peer
     * @throw     RuntimeError     Lost connection
     * @throw     InvalidArgument  Remote peer says it's a publisher but it can't be
     */
    PubPeerImpl(
            PubMgr&     pubMgr,
            PeerConnPtr conn)
        : BasePeerImpl(pubMgr, conn)
    {
        if (isRmtPub())
            throw INVALID_ARGUMENT("Remote peer " + rmtSrvrInfo.srvrAddr.to_string() +
                    " can't be publisher because I am: " + pubMgr.getSrvrInfo().to_string());
    }

    ~PubPeerImpl() noexcept {
        try {
            stopImpl(); // Idempotent
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }
    }

    P2pSrvrInfo::Tier getTier() const noexcept override {
        return 0; // A publisher's peer is always tier 0
    }

    void request(const ProdId prodId) override {
        throw LOGIC_ERROR("Shouldn't have been called");
    }
    void request(const DataSegId& segId) override {
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

PeerPtr Peer::create(
        PubMgr& pubPeerMgr,
        PeerConnPtr conn) {
    return PeerPtr(new PubPeerImpl(pubPeerMgr, conn));
}

/**************************************************************************************************/

/**
 * Subscriber's peer implementation. May be constructed server-side or client-side.
 */
class SubPeerImpl final : public BasePeerImpl
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
         * Tells a subscriber's peer manager about all requests in the queue in the order in which
         * they were added, then clears the queue.
         *
         * @param[in] peerMgr      Subscriber's peer manager
         * @param[in] rmtSockAddr  Socket address of the remote peer
         * @see SubP2pNode::missed(ProdId, SockAddr)
         * @see SubP2pNode::missed(DataSegId, SockAddr)
         */
        void drainTo(
                SubMgr&        subMgr,
                const SockAddr rmtSockAddr) {
            Guard guard{mutex};
            while (head) {
                if (head.id == Notice::Id::PROD_INDEX) {
                    subMgr.missed(head.prodId, rmtSockAddr);
                }
                else if (head.id == Notice::Id::DATA_SEG_ID) {
                    subMgr.missed(head.dataSegId, rmtSockAddr);
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

    SubMgr&        subMgr;      ///< Subscriber's peer manager
    RequestQ       requested;   ///< Requests sent to remote peer
    std::once_flag backlogFlag; ///< Ensures that the backlog is requested only once

    /**
     * Requests the backlog of data that this instance missed from the remote peer.
     */
    void requestBacklog() {
        const auto prodIds = subMgr.getProdIds(); // All complete products
        if (prodIds.size())
            /**
             * Informing the remote peer of data this instance doesn't know about is impossible;
             * therefore, the remote peer is informed of data that this instance has, which will
             * cause the remote peer to notify this instance of data that it has but that this
             * instance doesn't.
             */
            if (!(connected = peerConnPtr->request(prodIds)))
                ::sem_post(&stopSem);
    }

    /**
     * Received a datum from the remote peer. If the datum wasn't requested, then an exception is
     * thrown. Each request in the requested queue before the request for the given datum is removed
     * from the queue and passed to the peer manager as not satisfiable by the remote peer.
     * Otherwise, the datum is passed to the subscriber's peer manager and the pending request is
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
                subMgr.recvData(datum, rmtSockAddr);
                requested.pop();
                return;
            }
            /*
             * NB: A skipped response from the remote peer means that it doesn't have the requested
             * data.
             */
            if (iter->getType() == Notice::Id::DATA_SEG_ID) {
                subMgr.missed(iter->dataSegId, rmtSockAddr);
            }
            else if (iter->getType() == Notice::Id::PROD_INDEX) {
                subMgr.missed(iter->prodId, rmtSockAddr);
            }

            ++iter; // Must occur before `requested.pop()`
            requested.pop();
        }
        throw LOGIC_ERROR("Peer " + to_string() + " received unrequested datum " +
                datum.to_string());
    }

    /**
     * Executes this instance. Starts sending to and receiving from the remote peer. In particular,
     * requests the backlog of data-products that this instance missed.
     * @throw LogicError  The state isn't `INIT`
     */
    void startImpl() override {
        BasePeerImpl::startImpl();
        requestBacklog();
    }

public:
    /**
     * Constructs -- both client-side and server-side.
     *
     * @param[in] subMgr        Subscriber's peer manager
     * @param[in] conn          Pointer to peer-connection
     * @throw     LogicError    Destination port number is zero
     * @throw     SystemError   Couldn't connect. Bad failure.
     * @throw     RuntimeError  Couldn't connect. Might be temporary.
     */
    SubPeerImpl(
            SubMgr&      subMgr,
            PeerConnPtr  conn)
        : BasePeerImpl(subMgr, conn)
        , subMgr(subMgr)
        , requested()
        , backlogFlag()
    {}

    ~SubPeerImpl() noexcept {
        try {
            stopImpl();
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }
    }

    P2pSrvrInfo::Tier getTier() const noexcept override {
        return rmtSrvrInfo.getRmtTier();
    }

    void request(const ProdId prodId) override {
        LOG_DEBUG("Peer " + to_string() + " sending request for product " + prodId.to_string());
        requested.push(Notice{prodId});
        if ((connected = peerConnPtr->request(prodId))) {
            //LOG_TRACE("Peer %s requested information on product %s", to_string().data(),
                    //prodId.to_string().data());
        }
        else {
            ::sem_post(&stopSem);
        }
    }
    void request(const DataSegId& segId) override {
        LOG_DEBUG("Peer " + to_string() + " sending request for segment " + segId.to_string());
        requested.push(Notice{segId});
        if ((connected = peerConnPtr->request(segId))) {
            //LOG_TRACE("Peer %s requested information on data-segment %s", to_string().data(),
                    //segId.to_string().data());
        }
        else {
            ::sem_post(&stopSem);
        }
    }

    /**
     * Receives a notice about available product information. Notifies the subscriber's peer manager.
     * Requests the datum if told to do so by the peer manager. Called by the RPC layer.
     *
     * The first time this function is called, it tells the remote peer what complete data-products
     * it has.
     *
     * @param[in] prodId     Product index
     * @retval    false      Connection lost
     * @retval    true       Success
     */
    bool recvNotice(const ProdId prodId) override {
        LOG_DEBUG("Peer " + to_string() + " received notice about product " + prodId.to_string());
        //LOG_TRACE("Peer %s received notice about product %s", to_string().data(),
                //prodId.to_string().data());

        //std::call_once(backlogFlag, [&]{requestBacklog();});

        if (subMgr.recvNotice(prodId, rmtSockAddr)) {
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
     * Receives a notice about an available data segment. Notifies the subscriber's peer manager.
     * Requests the datum if told to do so by the peer manager. Called by the RPC layer.
     *
     * The first time this function is called, it tells the remote peer what complete data-products
     * it has.
     *
     * @param[in] dataSegId  Data segment ID
     * @retval    false      Connection lost
     * @retval    true       Success
     */
    bool recvNotice(const DataSegId dataSegId) override {
        LOG_DEBUG("Peer " + to_string() + " received notice about segment " + dataSegId.to_string());
        //LOG_TRACE("Peer %s received notice about data segment %s", to_string().data(),
                //dataSegId.to_string().data());

        //std::call_once(backlogFlag, [&]{requestBacklog();});

        if (subMgr.recvNotice(dataSegId, rmtSockAddr)) {
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
    void recvData(const ProdInfo prodInfo) override {
        LOG_DEBUG("Peer " + to_string() + " received product-info " + prodInfo.to_string());
        //LOG_TRACE("Peer %s received information on product %s",
                //to_string().data(), prodInfo.getId().to_string().data());
        processData<ProdInfo>(prodInfo);
    }
    /**
     * Receives a data segment from the remote peer.
     *
     * @param[in] dataSeg  Data segment
     */
    void recvData(const DataSeg dataSeg) override {
        LOG_DEBUG("Peer " + to_string() + " received segment " + dataSeg.to_string());
        //LOG_TRACE("Peer %s received data-segment %s", to_string().data(),
                //dataSeg.to_string().data());
        processData<DataSeg>(dataSeg);
    }

    void drainPending() override {
        requested.drainTo(subMgr, rmtSockAddr);
    }
};

PeerPtr Peer::create(
        SubMgr&      subMgr,
        PeerConnPtr& conn) {
    try {
        return PeerPtr{new SubPeerImpl{subMgr, conn}};
    }
    catch (const std::exception& ex) {
        LOG_WARN(ex, "Couldn't create peer for %s", conn->getRmtAddr().to_string().data());
        return PeerPtr();
    }
}

PeerPtr Peer::create(
        SubMgr&         subMgr,
        const SockAddr& srvrAddr) {
    auto conn = PeerConn::create(srvrAddr);
    return Peer::create(subMgr, conn);
}

/******************************************************************************/

/// An implementation of a P2P-server
/// @tparam PEER_MGR  The type of peer manager (i.e., publisher or subscriber)
template<typename PEER_MGR>
class P2pSrvrImpl : public P2pSrvr<PEER_MGR>
{
    PeerConnSrvrPtr peerConnSrvr;

public:
    /// Constructs
    P2pSrvrImpl(
            const SockAddr srvrAddr,
            const unsigned maxPendConn)
        : peerConnSrvr(PeerConnSrvr::create(srvrAddr, maxPendConn))
    {
        LOG_NOTE("Created P2P server " + to_string());
    }

    /// Constructs
    P2pSrvrImpl(PeerConnSrvrPtr peerConnSrvr)
        : peerConnSrvr(peerConnSrvr)
    {}

    SockAddr getSrvrAddr() const override {
        return peerConnSrvr->getSrvrAddr();
    }

    String to_string() const override {
        return getSrvrAddr().to_string();
    }

    /**
     * Accepts a connection from a remote client peer and returns a newly-constructed, server-side
     * counterpart.
     * @param[in] p2pMgr  The associated peer manager
     * @return            The server-side counterpart of the remote peer
     */
    PeerPtr accept(PEER_MGR& peerMgr) override {
        auto peerConn = peerConnSrvr->accept();
        return peerConn
                ? Peer::create(peerMgr, peerConn)
                : PeerPtr{};
    }

    /**
     * Causes `accept()` to return a false object.
     * @see accept()
     */
    void halt() override {
        peerConnSrvr->halt();
    }
};

using PubP2pSrvrImpl = P2pSrvrImpl<Peer::PubMgr>; ///< Implementation of publisher's P2P-server
using SubP2pSrvrImpl = P2pSrvrImpl<Peer::SubMgr>; ///< Implementation of subscriber's P2P-server

/**
 * Returns a new instance of a P2P-server for a publisher.
 * @param[in] srvrAddr     Socket address for the server
 * @param[in] maxPendConn  Maximum number of pending connections from remote peers
 * @return A new instance of a P2P-server for a publisher
 */
template<>
PubP2pSrvrPtr PubP2pSrvr::create(
        const SockAddr  srvrAddr,
        const unsigned  maxPendConn) {
    return PubP2pSrvrPtr{new PubP2pSrvrImpl(srvrAddr, maxPendConn)};
}

/**
 * Returns a new instance of a P2P-server for a subscriber.
 * @param[in] peerConnSrvr  Peer-connection server
 * @return A new instance of a P2P-server for a subscriber
 */
template<>
SubP2pSrvrPtr SubP2pSrvr::create(const PeerConnSrvrPtr peerConnSrvr) {
    return SubP2pSrvrPtr{new SubP2pSrvrImpl(peerConnSrvr)};
}

/**
 * Returns a new instance of a P2P-server for a subscriber.
 * @param[in] srvrAddr     Socket address for the server
 * @param[in] maxPendConn  Maximum number of pending connections from remote peers
 * @return A new instance of a P2P-server for a subscriber
 */
template<>
SubP2pSrvrPtr SubP2pSrvr::create(
        const SockAddr srvrAddr,
        const unsigned maxPendConn) {
    return SubP2pSrvrPtr{new SubP2pSrvrImpl(srvrAddr, maxPendConn)};
}

} // namespace
