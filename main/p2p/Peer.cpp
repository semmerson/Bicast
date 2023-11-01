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

#include "Peer.h"

#include "BicastProto.h"
#include "CommonTypes.h"
#include "error.h"
#include "logging.h"
#include "Notice.h"
#include "ThreadException.h"

#include <atomic>
#include <mutex>
#include <queue>
#include <semaphore.h>
#include <unordered_map>

namespace bicast {

/**
 * Abstract base implementation of the `Peer` interface.
 */
class BasePeerImpl : public Peer
{
protected:
    /// State of this instance
    enum class State {
        INIT,    ///< Constructed
        STARTED, ///< Started
        STOPPED  ///< Stopped
    }                  state;         ///< State of this instance
    mutable Mutex      stateMutex;    ///< Protects state changes

private:
    using NoticeQ    = std::queue<Notice>;
    using AtomicTime = std::atomic<SysTimePoint>;

    mutable Mutex      noticeMutex;       ///< For accessing notice queue
    mutable Cond       noticeCond;        ///< For accessing notice queue
    Thread             noticeThread;      ///< For sending notices
    bool               stopNotices;       ///< Stop the notice writer?
    Thread             connThread;        ///< Thread for running the peer-connection
    std::once_flag     backlogFlag;       ///< Ensures that a backlog is sent only once
    BaseMgr&           basePeerMgr;       ///< Associated peer manager
    NoticeQ            noticeQ;           ///< Notice queue
    ThreadEx           threadEx;          ///< Internal thread exception
    SockAddr           lclSockAddr;       ///< Local (notice) socket address
    const SysDuration  heartbeatInterval; ///< Time between heartbeats
    AtomicTime         lastSendTime;      ///< Time of last sending
    Thread             heartbeatThread;   ///< Thread on which heartbeats are sent

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

    /**
     * Adds a heartbeat notice to the notice queue.
     * @retval    true     Success
     * @retval    false    Connection lost
     */
    bool addHeartbeat() {
        threadEx.throwIfSet();

        if (connected) {
            LOG_ASSERT(!noticeMutex.try_lock());

            //LOG_TRACE("Peer %s is adding heartbeat", to_string().data());
            auto heartbeat = Notice::createHeartbeat();
            noticeQ.push(heartbeat);
            noticeCond.notify_all();
        }

        return connected;
    }

    /**
     * Runs the heartbeat thread. Adds a heartbeat notice to the queue if nothing has been sent in
     * duration `heartbeatInterval`. The heartbeat PDU has no payload.
     */
    void runHeartbeat() {
        try {
            lastSendTime = SysClock::now();
            for (;;) {
                SysTimePoint nextHeartbeat = lastSendTime.load() + heartbeatInterval;

                std::this_thread::sleep_until(nextHeartbeat);

                if (SysClock::now() >= lastSendTime.load() + heartbeatInterval) {
                    Guard guard{noticeMutex}; // Might throw std::system_error if already locked
                    if (noticeQ.empty() && !addHeartbeat()) // addHeartbeat() can throw something
                        break;
                    lastSendTime = SysClock::now();
                }
            }
        }
        catch (const std::exception& ex) {
            log_error(ex);
            setException();
        }
    }

    /**
     * Stops and joins the heartbeat thread. Idempotent.
     */
    void stopHeartbeat() {
        if (heartbeatThread.joinable()) {
            ::pthread_cancel(heartbeatThread.native_handle());
            heartbeatThread.join();
        }
    }

    /**
     * Handles the backlog of products missing from the remote peer. Meant to be the start-routine
     * of a separate thread. Calls `setException()` on error.
     *
     * @param[in] prodIds  Identifiers of complete products that the remote per has
     * @see notify()
     * @see setException()
     */
    void runBacklog(ProdIdSet prodIds) {
        try {
            // Products that the remote peer doesn't have
            const ProdIdSet missing = basePeerMgr.subtract(prodIds);
            const auto      maxSegSize = DataSeg::getMaxSegSize();

            /*
             * The regular P2P mechanism is used to notify the remote peer of products that it's
             * missing
             */
            for (auto iter =  missing.begin(), end = missing.end(); iter != end; ++iter) {
                const ProdId prodId = *iter;
            //for (const auto prodId : missing) {
                notify(prodId); // Hidden by Peer::notify()? Can't be; that function's pure virtual

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
            setException();
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
                    noticeCond.wait(lock, [&]{return stopNotices || !noticeQ.empty();});

                    if (stopNotices)
                        break;

                    notice = noticeQ.front();
                    noticeQ.pop();
                }

                switch (notice.id) {
                    case Notice::Id::DATA_SEG_ID: {
                        //LOG_TRACE("Notifying about data-segment " + notice.dataSegId.to_string());
                        connected = peerConnPtr->notify(notice.dataSegId);
                        //LOG_TRACE("Notified");
                        break;
                    }
                    case Notice::Id::PROD_ID: {
                        //LOG_TRACE("Notifying about product " + notice.prodId.to_string());
                        connected = peerConnPtr->notify(notice.prodId);
                        //LOG_TRACE("Notified");
                        break;
                    }
                    case Notice::Id::P2P_SRVR_INFO: {
                        //LOG_TRACE("Notifying about P2P server " + notice.srvrInfo.to_string());
                        connected = peerConnPtr->notify(notice.srvrInfo);
                        //LOG_TRACE("Notified");
                        break;
                    }
                    case Notice::Id::HEARTBEAT: {
                        connected = peerConnPtr->sendHeartbeat();
                        break;
                    }
                    default: {
                        throw LOGIC_ERROR("Notice ID is unknown: " +
                                std::to_string((int)notice.id));
                    }
                    resetHeartbeat();
                }
            }

            setDisconnected();
        }
        catch (const std::exception& ex) {
            setException();
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
            {
                Guard guard{noticeMutex};
                stopNotices = true;
                noticeCond.notify_all();
            }
            noticeThread.join();
        }
    }

    /**
     * Executes the peer-connection.
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

    /// Starts the thread that runs the connection with the remote peer.
    void startConn() {
        connThread = Thread(&BasePeerImpl::runConn, this);
    }

    /**
     * Stops the connection with the remote peer. Idempotent.
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
     * @pre               The state mutex is locked
     * @post              The state mutex is locked
     */
    virtual void startImpl() {
        LOG_ASSERT(!stateMutex.try_lock());
        startNoticeWriter();
        try {
            heartbeatThread = Thread(&BasePeerImpl::runHeartbeat, this);
            try {
                startConn();
            }
            catch (const std::exception& ex) {
                stopHeartbeat();
                throw;
            } // Notice writer thread started
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
     * @pre               The state mutex is locked
     * @post              The state is STOPPED
     * @post              The state mutex is locked
     */
    void stopImpl() {
        LOG_ASSERT(!stateMutex.try_lock());
        stopConn();         // Idempotent
        stopHeartbeat();    // Idempotent
        stopNoticeWriter(); // Idempotent
        state = State::STOPPED;
    }

    /**
     * Adds a notice to be sent iff the remote peer isn't the publisher.
     * @param[in] notice   The notice to be sent
     * @retval    true     Success
     * @retval    false    Connection lost
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

    /**
     * Resets the heartbeat timer.
     */
    inline void resetHeartbeat() {
        lastSendTime = SysClock::now();
    }

public:
    /**
     * Constructs.
     *
     * @param[in] baseMgr            Base peer manager
     * @param[in] conn               Connection with remote peer
     * @param[in] heartbeatInterval  Time between sending heartbeats to the remote peer
     * @throw SystemError            System failure
     * @throw RuntimeError           Couldn't exchange server information with remote peer
     */
    BasePeerImpl(
            BaseMgr&     baseMgr,
            PeerConnPtr  conn,
            const SysDuration heartbeatInterval = std::chrono::seconds(30))
        : state(State::INIT)
        , stateMutex()
        , noticeMutex()
        , noticeCond()
        , noticeThread()
        , stopNotices(false)
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
        , heartbeatInterval(heartbeatInterval)
        , lastSendTime(SysClock::now())
        , heartbeatThread()
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
        if (state == State::STARTED)
            stopImpl();
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

    virtual Tier getTier() const noexcept =0;

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
        /*
         * Because a condition variable isn't async-signal-safe, its behavior is simulated using a
         * semaphore (which *is* async-signal-safe).
         */
        {
            Guard guard{stateMutex};
            LOG_ASSERT(state == State::INIT);
            startImpl();
        }
        try {
            /*
             * Blocks until
             *   - `connected` is false
             *   - `halt()` is called
             *   - `threadEx` is true
             */
            ::sem_wait(&stopSem);
            {
                Guard guard{stateMutex};
                LOG_TRACE("connected=" + std::to_string(connected) + "; threadEx=" +
                        (threadEx ? "true" : "false"));
                stopImpl();
            }
            threadEx.throwIfSet();
        }
        catch (const std::exception& ex) {
            {
                Guard guard{stateMutex};
                if (state == State::STARTED)
                    stopImpl();
            }
            throw;
        }
    }

    void halt() override {
        LOG_TRACE("Called");
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
        auto notice = Notice{srvrInfo};
        addNotice(notice);
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
            if (connected) {
                resetHeartbeat();
            }
            else {
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
            if (connected = peerConnPtr->send(dataSeg)) {
                resetHeartbeat();
            }
            else {
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

    Tier getTier() const noexcept override {
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
    /// A request for a datum. Implemented as a tagged union.
    struct Request {
        /// Identifier of the type of request
        enum class Tag {
            UNSET,          ///< Request was default constructed
            PROD_ID,        ///< Product ID
            DATA_SEG_ID     ///< Data segment ID
        } tag; ///< Identifier of the type of request
        union {
            ProdId      prodId;    ///< Product ID
            DataSegId   dataSegId; ///< Data segment ID
        };

        /// Default constructs
        Request() noexcept
            : tag(Tag::UNSET)
        {}

        /**
         * Constructs a request about an available product.
         * @param[in] prodId  The product's ID
         */
        explicit Request(const ProdId prodId) noexcept
            : tag(Tag::PROD_ID)
            , prodId(prodId)
        {}

        /**
         * Constructs a request about an available data segment.
         * @param[in] dataSegId The data segment's ID
         */
        explicit Request(const DataSegId dataSegId) noexcept
            : tag(Tag::DATA_SEG_ID)
            , dataSegId(dataSegId)
        {}

        /**
         * Copy constructs.
         * @param[in] that  The other instance
         */
         Request(const Request& that) noexcept {
            tag = that.tag;
            switch (tag) {
            case Tag::PROD_ID:        new (&prodId)    auto(that.prodId);    break;
            case Tag::DATA_SEG_ID:    new (&dataSegId) auto(that.dataSegId); break;
            default:                                                         break;
            }
        }

        /// Destroys
        ~Request() noexcept {
        }

        /**
         * Copy assigns.
         * @param[in] rhs  The other instance
         * @return         A reference to this just-assigned instance
         */
        Request& operator=(const Request& rhs) noexcept {
            tag = rhs.tag;
            switch (tag) {
            case Tag::PROD_ID:        new (&prodId)    auto(rhs.prodId);    break;
            case Tag::DATA_SEG_ID:    new (&dataSegId) auto(rhs.dataSegId); break;
            default:                                                        break;
            }
            return *this;
        }

        /**
         * Indicates if this instance is valid (i.e., wasn't default constructed).
         * @retval true     This instance is valid
         * @retval false    This instance is not valid
         */
        inline operator bool() const noexcept {
            return tag != Tag::UNSET;
        }

        /**
         * Returns the string representation of this instance.
         * @return The string representation of this instance
         */
        String to_string() const {
            switch (tag) {
                case Tag::PROD_ID:        return prodId.to_string();
                case Tag::DATA_SEG_ID:    return dataSegId.to_string();
                default:                  return "<unset>";
            }
        }

        /// Hash function class
        struct Hash {
            /**
             * Returns the hash value of a Request.
             * @param[in] request  The Request
             * @return             The corresponding hash value
             */
            size_t operator()(const Request& request) const noexcept {
                switch (request.tag) {
                    case Tag::PROD_ID:     return request.prodId.hash();
                    case Tag::DATA_SEG_ID: return request.dataSegId.hash();
                    default:               return static_cast<size_t>(0);
                }
            }
        };

        /**
         * Indicates if this instance is equal to another.
         * @param[in] rhs      The other instance
         * @retval    true     This instance is equal to the other
         * @retval    false    This instance is not equal to the other
         */
        inline bool operator==(const Request& rhs) const noexcept {
            if (tag != rhs.tag)     return false;
            switch (tag) {
                case Tag::UNSET:       return true;
                case Tag::PROD_ID:     return prodId == rhs.prodId;
                case Tag::DATA_SEG_ID: return dataSegId == rhs.dataSegId;
                default:               return false;
            }
        }

        /**
         * Tells the subscriber's P2P manager about a datum that the remote peer doesn't have.
         * @param[in] subMgr       Subscriber's P2P manager
         * @param[in] rmtSockAddr  Socket address of the remote peer
         */
        void missed(
                SubMgr&   subMgr,
                SockAddr& rmtSockAddr) {
            switch (tag) {
            case Request::Tag::PROD_ID:     subMgr.missed(prodId, rmtSockAddr);    break;
            case Request::Tag::DATA_SEG_ID: subMgr.missed(dataSegId, rmtSockAddr); break;
            default:                                                               break;
            }
        }
    };

    /**
     * A thread-unsafe, linked-map of datum requests.
     */
    class RequestQ {
        /// The value component references the next entry in a linked list
        using Requests = std::unordered_map<Request, Request, Request::Hash>;

        Requests      requests;
        Request       head;
        Request       tail;

    public:
        /**
         * Default constructs.
         */
        RequestQ()
            : requests()
            , head()
            , tail()
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
            requests.emplace(request, Request{});
            if (tail)
                requests[tail] = request;
            if (!head)
                head = request;
            tail = request;
            return requests.size();
        }

        size_t count(const Request& request) {
            return requests.count(request);
        }

        /**
         * Removes and returns the request at the head of the queue.
         *
         * @pre    Mutex is locked
         * @pre    Queue is not empty
         * @return The former head of the queue
         * @post   Mutex is locked
         */
        Request removeHead() {
            auto oldHead = head;
            auto newHead = requests[head];
            requests.erase(head);
            head = newHead;
            if (!head)
                tail = head; // Queue is empty
            return oldHead;
        }

        /**
         * Tells a subscriber's peer manager about all requests in the queue in the order in which
         * they were added, then clears the queue.
         *
         * @param[in] subMgr       Subscriber's peer manager
         * @param[in] rmtSockAddr  Socket address of the remote peer
         * @see SubP2pNode::missed(ProdId, SockAddr)
         * @see SubP2pNode::missed(DataSegId, SockAddr)
         */
        void drainTo(
                SubMgr&        subMgr,
                const SockAddr rmtSockAddr) {
            while (head) {
                if (head.tag == Request::Tag::PROD_ID) {
                    subMgr.missed(head.prodId, rmtSockAddr);
                }
                else if (head.tag == Request::Tag::DATA_SEG_ID) {
                    subMgr.missed(head.dataSegId, rmtSockAddr);
                }
                head = requests[head];
            }
            requests.clear();
            tail = head;
        }
    }; // RequestQ

    SubMgr&        subMgr;      ///< Subscriber's peer manager
    RequestQ       requested;   ///< Requests sent to remote peer

    /**
     * Requests the backlog of data that this instance missed from the remote peer.
     */
    void requestBacklog() {
        const auto prodIds = subMgr.getProdIds(); // All complete products
        if (prodIds.size()) {
            /**
             * Informing the remote peer of data this instance doesn't know about is impossible;
             * therefore, the remote peer is informed of data that this instance has, which will
             * cause the remote peer to notify this instance of data that it has but that this
             * instance doesn't.
             */
            if (connected = peerConnPtr->request(prodIds)) {
                resetHeartbeat();
            }
            else {
                ::sem_post(&stopSem);
            }
        }
    }

    /**
     * Receives a datum from the remote peer. If the datum wasn't requested, then an exception is
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
    void processDatum(const DATUM datum) {
        //LOG_DEBUG("Processing datum %s", datum.to_string().data());

        const auto&   id = datum.getId(); // Either ProdId or DataSegId
        const Request request{id};
        Guard         guard{stateMutex}; // To keep `requested` consistent

        if (requested.count(request) == 0) {
            //LOG_ERROR("Peer " + to_string() + " received unrequested datum " + datum.to_string());
            throw LOGIC_ERROR("Peer " + to_string() + " received unrequested datum " +
                    datum.to_string());
        }

        // The given request exists
        for (auto head = requested.removeHead(); head; head = requested.removeHead()) {
            if (head == request) {
                subMgr.recvData(datum, rmtSockAddr);
                break;
            }
            head.missed(subMgr, rmtSockAddr); // DEADLOCK if this tries to lock stateMutex
        }
    }

    /**
     * Executes this instance. Starts sending to and receiving from the remote peer. In particular,
     * requests the backlog of data-products that this instance missed.
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
    {}

    Tier getTier() const noexcept override {
        return rmtSrvrInfo.getRmtTier();
    }

    void request(const ProdId prodId) override {
        LOG_DEBUG("Peer " + to_string() + " sending request for product " + prodId.to_string());
        {
            Guard guard{stateMutex}; // To keep `requested` consistent
            requested.push(Request{prodId});
        }
        if (connected = peerConnPtr->request(prodId)) {
            //LOG_TRACE("Peer %s requested information on product %s", to_string().data(),
                    //prodId.to_string().data());
            resetHeartbeat();
        }
        else {
            ::sem_post(&stopSem);
        }
    }
    void request(const DataSegId& segId) override {
        LOG_DEBUG("Peer " + to_string() + " sending request for segment " + segId.to_string());
        {
            Guard guard{stateMutex}; // To keep `requested` consistent
            requested.push(Request{segId});
        }
        if (connected = peerConnPtr->request(segId)) {
            //LOG_TRACE("Peer %s requested information on data-segment %s", to_string().data(),
                    //segId.to_string().data());
            resetHeartbeat();
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
        processDatum<ProdInfo>(prodInfo);
    }
    /**
     * Receives a data segment from the remote peer.
     *
     * @param[in] dataSeg  Data segment
     */
    void recvData(const DataSeg dataSeg) override {
        try {
            LOG_DEBUG("Peer " + to_string() + " received segment " + dataSeg.to_string());
            //LOG_TRACE("Peer %s received data-segment %s", to_string().data(),
                    //dataSeg.to_string().data());
            processDatum<DataSeg>(dataSeg);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Peer" + to_string() +
                    " received unrequested segment"));
        }
    }

    void drainPending() override {
        Guard guard{stateMutex}; // To keep `requested` consistent
        requested.drainTo(subMgr, rmtSockAddr);
    }
};

PeerPtr Peer::create(
        SubMgr&      subMgr,
        PeerConnPtr& conn) {
    try {
        return PeerPtr(new SubPeerImpl{subMgr, conn});
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
     * @param[in] peerMgr  The associated peer manager
     * @return             The server-side counterpart of the remote peer
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

template<>
PubP2pSrvrPtr PubP2pSrvr::create(
        const SockAddr  srvrAddr,
        const unsigned  maxPendConn) {
    return PubP2pSrvrPtr{new PubP2pSrvrImpl(srvrAddr, maxPendConn)};
}

template<>
SubP2pSrvrPtr SubP2pSrvr::create(const PeerConnSrvrPtr peerConnSrvr) {
    return SubP2pSrvrPtr{new SubP2pSrvrImpl(peerConnSrvr)};
}

template<>
SubP2pSrvrPtr SubP2pSrvr::create(
        const SockAddr srvrAddr,
        const unsigned maxPendConn) {
    return SubP2pSrvrPtr{new SubP2pSrvrImpl(srvrAddr, maxPendConn)};
}

} // namespace
