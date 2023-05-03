/**
 * @file PeerConn.cpp
 * The connection between two peers.
 * Interfaces between a Peer and the Rpc layer by hiding the number of socket connections and
 * threads from both.
 *
 *  Created on: Apr 25, 2023
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2023 University Corporation for Atmospheric Research
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
#include "PeerConn.h"
#include "Rpc.h"
#include "ThreadException.h"
#include "Xprt.h"

#include <poll.h>
#include <queue>
#include <semaphore.h>
#include <unordered_map>

namespace hycast {

using XprtArray = std::array<Xprt, 3>; ///< Type of transport array for a connection to a peer

/**
 * Implementation of a peer-connection. This particular implementation uses three sockets and three
 * threads on which the sockets are read and the associated local peer called.
 */
class PeerConnImpl : public PeerConn
{
    const bool         iAmClient;     ///< This instance initiated the connection?
    /*
     * If a single transport is used for asynchronous communication and reading and writing occur on
     * the same thread, then deadlock will occur if both receive buffers are full and each end is
     * trying to write. To obviate this, three transports are used and a thread that reads from one
     * transport will write to another. The pipeline might block for a while as messages are
     * processed, but it won't deadlock.
     */
    Xprt               noticeXprt;    ///< Notice transport
    Xprt               requestXprt;   ///< Request and tracker information transport
    Xprt               dataXprt;      ///< Data transport
    SockAddr           rmtSockAddr;   ///< Remote (notice) socket address
    SockAddr           lclSockAddr;   ///< Local (notice) socket address
    mutable sem_t      stopSem;       ///< For async-signal-safe stopping
    ThreadEx           threadEx;      ///< Holds exception thrown by an internal thread
    Thread             requestReader; ///< For receiving requests
    Thread             noticeReader;  ///< For receiving notices
    Thread             dataReader;    ///< For receiving data
    Rpc::Pimpl         rpc;           ///< Remote procedure call object
    Peer::Pimpl        peer;          ///< Associated peer

    static void asyncConnectXprt(
            const SockAddr& srvrAddr,
            Xprt&           xprt,
            const int       timeout) {
        try {
            const auto  srvrInetAddr = srvrAddr.getInetAddr();
            TcpClntSock sock{srvrInetAddr.getFamily()}; // Unbound & unconnected socket

            sock.bind(SockAddr(srvrInetAddr.getWildcard(), 0));
            sock.makeNonBlocking();
            sock.connect(srvrAddr); // Immediately sets local socket address

            struct pollfd pfd;
            pfd.fd = sock.getSockDesc();
            pfd.events = POLLOUT;
            int status = ::poll(&pfd, 1, (timeout < 0) ? -1 : timeout);

            if (status == -1)
                throw SYSTEM_ERROR("poll() failure");
            if (status == 0)
                throw RUNTIME_ERROR("Couldn't connect to " + srvrAddr.to_string() + " in " +
                        std::to_string(timeout) + " ms");

            if (pfd.revents & (POLLHUP | POLLERR))
                throw SYSTEM_ERROR("Couldn't connect to " + srvrAddr.to_string());
            if (pfd.revents & POLLOUT)
                sock.makeBlocking();

            xprt = Xprt{sock};
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }
    }

    /**
     * Connects transports asynchronously to a remote counterpart.
     *
     * @param[in]  srvrAddr  Socket address of remote server
     * @param[out] xprts     Transports to be set
     * @param[in]  timeout   Timeout, in ms, to connect all transports
     */
    void asyncConnect(
            const SockAddr& srvrAddr,
            XprtArray&      xprts,
            const int       timeout) {
        const int numXprts = xprts.size();
        Thread    threads[numXprts];

        for (int i = 0; i < numXprts; ++i)
            threads[i] = Thread(&PeerConnImpl::asyncConnectXprt, srvrAddr, std::ref(xprts[i]),
                    timeout);

        for (int i = 0; i < numXprts; ++i) {
            threads[i].join();
            if (!xprts[i])
                throw RUNTIME_ERROR("Couldn't connect transports to " + srvrAddr.to_string());
        }
    }

    /**
     * Sends transport ID-s to remote counterpart.
     *
     * @param[in] xprts  Transports
     * @param[in] port   Port number of notice transport
     */
    void sendXprtIds(
            XprtArray&      xprts,
            const in_port_t port) {
        for (uint8_t xprtId = 0; xprtId < xprts.size(); ++xprtId)
            if (!xprts[xprtId].write(port) || !xprts[xprtId].write(xprtId))
                throw RUNTIME_ERROR("Couldn't send transport ID-s");
    }

    /**
     * Connects this client-side instance to a remote counterpart. Called by client-side
     * constructor.
     * @param[in] srvrAddr         Address of remote RPC server
     * @param[in] timeout          Timeout, in ms, for connecting with remote peer. <=0 => system's
     *                             default timeout;
     * @throw     InvalidArgument  Destination port number is zero
     * @throw     RuntimeError     Couldn't connect to remote within timeout
     * @throw     SystemError      System failure
     */
    void connect(
            const SockAddr& srvrAddr,
            int             timeout) {
        // Keep consonant with `setXprts()`.

        XprtArray xprts;

        asyncConnect(srvrAddr, xprts, timeout);

        const auto port = xprts[0].getLclAddr().getPort();

        sendXprtIds(xprts, port);

        noticeXprt = xprts[0];
        requestXprt = xprts[1];
        dataXprt = xprts[2];
    }

    /**
     * Sets the transports by reading transport ID-s sent by remote counterpart. Called by
     * server-side constructor.
     * @param[in] xprts  Server-side constructed transports
     * @see connect()
     */
    void setXprts(XprtArray xprts) {
        // Keep consonant with `connect()`.

        std::array<int, 3> xprtIndexes;

        for (uint8_t index = 0; index < 3; ++index) {
            uint8_t xprtId;
            if (!xprts[index].read(xprtId))
                throw RUNTIME_ERROR("Couldn't read from " + xprts[index].to_string());
            xprtIndexes[xprtId] = index;
        }

        noticeXprt  = xprts[xprtIndexes[0]];
        requestXprt = xprts[xprtIndexes[1]];
        dataXprt    = xprts[xprtIndexes[2]];
    }

    /// Performs common initialization on this instance
    void init() {
        if (::sem_init(&stopSem, 0, 0) == -1)
            throw SYSTEM_ERROR("Couldn't initialize semaphore");

        rmtSockAddr = noticeXprt.getRmtAddr();
        lclSockAddr = noticeXprt.getLclAddr();
    }

    /**
     * Reads and processes RPC messages from the remote peer. Doesn't return until either the
     * connection is lost or an exception is thrown. Meant to be the start routine of a separate
     * thread.
     *
     * @param[in] xprt   Transport
     */
    void runReader(Xprt& xprt) {
        // TODO: Make the priority of this thread greater than the multicast sending thread
        try {
            LOG_TRACE("Executing reader");
            while (rpc->process(xprt, *peer.get()))
                ;
            // Connection lost
            ::sem_post(&stopSem);
        }
        catch (const std::exception& ex) {
            log_error(ex);
            threadEx.set(ex);
            ::sem_post(&stopSem);
        }
    }

    /**
     * Starts the internal threads.
     */
    void startThreads() {
        noticeReader = Thread(&PeerConnImpl::runReader, this, std::ref(noticeXprt));
        try {
            requestReader = Thread(&PeerConnImpl::runReader, this, std::ref(requestXprt));
            try {
                dataReader = Thread(&PeerConnImpl::runReader, this, std::ref(dataXprt));
            }
            catch (const std::exception& ex) {
                requestXprt.shutdown();
                requestReader.join();
                throw;
            }
        }
        catch (const std::exception& ex) {
            noticeXprt.shutdown();
            noticeReader.join();
            throw;
        }
    }

    /**
     * Stops the internal threads.
     */
    void stopThreads() {
        // Idempotent
        dataXprt.shutdown();
        requestXprt.shutdown();
        noticeXprt.shutdown();

        dataReader.join();
        requestReader.join();
        noticeReader.join();
    }

    /**
     * Default constructs.
     */
    PeerConnImpl(const bool iAmClient)
        : iAmClient(iAmClient)
        , noticeXprt()
        , requestXprt()
        , dataXprt()
        , rmtSockAddr()
        , lclSockAddr()
        , stopSem()
        , requestReader()
        , noticeReader()
        , dataReader()
        , rpc(Rpc::create())
        , peer()
    {}

public:
    /**
     * Constructs a client-side instance.
     * @param[in] srvrAddr  Socket address of the remote P2P-server
     * @param[in] timeout   Timeout in ms. <=0 => System's default timeout.
     */
    PeerConnImpl(
            const SockAddr& srvrAddr,
            const int       timeout)
        : PeerConnImpl(true)
    {
        LOG_TRACE("Constructing client-side");
        connect(srvrAddr, timeout); // Sets transports
        init();
    }

    /**
     * Constructs a server-side instance.
     * @param[in,out] xprts   Server-side constructed transports comprising the connection
     */
    explicit PeerConnImpl(XprtArray xprts)
        : PeerConnImpl(false)
    {
        LOG_TRACE("Constructing server-side");
        setXprts(xprts);
        init();
    }

    ~PeerConnImpl() {
        ::sem_destroy(&stopSem);
    }

    /**
     * Indicates if instance initiated the connection.
     * @retval true   This instance initiated the connection
     * @retval false  This instance did not initiate the connection
     */
    bool isClient() const noexcept override {
        return iAmClient;
    }

    SockAddr getLclAddr() const noexcept override {
        return lclSockAddr;
    }

    SockAddr getRmtAddr() const noexcept override {
        return rmtSockAddr;
    }

    String to_string() const override {
        return "{lcl=" + lclSockAddr.to_string() + ", rmt=" + rmtSockAddr.to_string() + "}";
    }

    void setPeer(Peer::Pimpl& peer) noexcept override {
        LOG_ASSERT(!this->peer);

        this->peer = peer;
    }

    void run() override {
        if (dataReader.joinable() || requestReader.joinable() || noticeReader.joinable())
            throw LOGIC_ERROR("Peer-connection already started");

        startThreads();
        ::sem_wait(&stopSem); // Blocks until connection lost, `halt()` called or `threadEx` set
        stopThreads();
        threadEx.throwIfSet();
    }

    void halt() override {
        int semval = 0;
        ::sem_getvalue(&stopSem, &semval);
        if (semval < 1)
            ::sem_post(&stopSem);
    }

    // Notices:

    bool add(const P2pSrvrInfo& srvrInfo) override {
        return rpc->add(noticeXprt, srvrInfo);
    }

    bool add(const Tracker& tracker) override {
        return rpc->add(noticeXprt, tracker);
    }

    bool notify(const ProdId prodId) override {
        return rpc->notify(noticeXprt, prodId);
    }

    bool notify(const DataSegId dataSegId) override {
        return rpc->notify(noticeXprt, dataSegId);
    }

    // Requests:

    bool request(const ProdId prodId) override {
        return rpc->request(requestXprt, prodId);
    }

    bool request(const DataSegId dataSegId) override {
        return rpc->request(requestXprt, dataSegId);
    }

    bool request(const ProdIdSet& prodIds) override {
        return rpc->request(requestXprt, prodIds);
    }

    // Data:

    bool send(const ProdInfo prodInfo) override {
        return rpc->send(dataXprt, prodInfo);
    }

    bool send(const DataSeg dataSeg) override {
        return rpc->send(dataXprt, dataSeg);
    }
};

PeerConn::Pimpl PeerConn::create(
        const SockAddr& srvrAddr,
        const int       timeout)
{
    return PeerConn::Pimpl{new PeerConnImpl(srvrAddr, timeout)};
}

/**************************************************************************************************/

/// Implementation of a server for peer-connections
class PeerConnSrvrImpl : public PeerConnSrvr
{
private:
    using Pimpl = std::shared_ptr<PeerConnSrvrImpl>;

    /**
     * Factory for creating peer-connection instances from transports.
     */
    class PeerConnFactory
    {
        struct Entry {
            int n;
            XprtArray xprts;
            Entry()
                : n(0)
                , xprts()
            {}
        };

        std::unordered_map<SockAddr, Entry> peerConns;

        inline SockAddr getKey(
                const Xprt      xprt,
                const in_port_t port) {
            return xprt.getRmtAddr().clone(port);
        }

    public:
        /**
         * Constructs.
         */
        PeerConnFactory()
            : peerConns()
        {}

        /**
         * Adds an individual transport to a server-side peer-connection. If the addition completes
         * the connection, then it removed from this instance.
         *
         * @param[in]  xprt        Individual transport
         * @param[in]  noticePort  Port number of the notification transport
         * @retval     false       Connection is not complete
         * @retval     true        Connection is complete
         * @threadsafety           Unsafe
         */
        bool add(Xprt xprt, in_port_t noticePort) {
            // TODO: Limit number of outstanding connections
            // TODO: Purge old entries
            auto& entry = peerConns[getKey(xprt, noticePort)];
            entry.xprts[entry.n++] = xprt;
            return entry.n == 3;
        }

        XprtArray get(
                const Xprt      xprt,
                const in_port_t noticePort) {
            const auto key = getKey(xprt, noticePort);
            auto       xprts = peerConns.at(key).xprts;
            peerConns.erase(key);
            return xprts;
        }
    };

    mutable Mutex   mutex;           ///< State-protecting mutex
    mutable Cond    cond;            ///< To support inter-thread communication
    PeerConnFactory peerConnFactory; ///< Combines 3 unicast connections into one peer-connection
    TcpSrvrSock     srvrSock;        ///< Socket on which this instance listens

    using PeerConnQ = std::queue<PeerConn::Pimpl>;
    PeerConnQ       acceptQ;         ///< Queue of accepted peer-connections

    int             maxPendConn;     ///< Maximum number of pending connections
    Thread          acceptThread;    ///< Accepts incoming sockets

    /**
     * Executes on a separate thread.
     *
     * @param[in] sock  Newly-accepted socket
     */
    void processSock(TcpSock sock) {
        LOG_TRACE("Starting to process a socket");
        in_port_t noticePort;
        Xprt      xprt{sock}; // Might take a while depending on `Xprt`

        if (xprt.read(noticePort)) { // Might take a while
            // The rest is fast
            Guard guard{mutex};

            // TODO: Remove old, stale entries from accept-queue

            if (acceptQ.size() < maxPendConn) {
                LOG_TRACE("Adding transport to factory");
                if (peerConnFactory.add(xprt, noticePort)) {
                    LOG_TRACE("Emplacing peer-connection in queue");
                    acceptQ.emplace(new PeerConnImpl(peerConnFactory.get(xprt, noticePort)));
                    cond.notify_one();
                }
            }
        } // Read port number of notice transport
    }

    void acceptSocks() {
        try {
            LOG_TRACE("Starting to accept sockets");
            for (;;) {
                LOG_TRACE("Accepting a socket");
                auto sock = srvrSock.accept();
                if (!sock) {
                    // The server's listening socket has been shut down
                    Guard guard{mutex};
                    PeerConnQ  emptyQ;
                    acceptQ.swap(emptyQ); // Empties accept-queue
                    acceptQ.push(PeerConn::Pimpl{}); // Will test false
                    cond.notify_one();
                    break;
                }

                processSock(sock);
                LOG_TRACE("Processed socket %s", sock.to_string().data());
            }
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }
    }

public:
    /**
     * Constructs from the local address for the RPC-server.
     *
     * @param[in] srvrAddr     Socket address for the server. Must not be wildcard. A port number of
     *                         zero obtains a system chosen one.
     * @param[in] maxPendConn  Maximum number of pending connections
     * @throw InvalidArgument  Server's IP address is wildcard
     * @throw InvalidArgument  Backlog argument is zero
     */
    PeerConnSrvrImpl(
            const SockAddr&   srvrAddr,
            const int         maxPendConn)
        : mutex()
        , cond()
        , peerConnFactory()
        , srvrSock()
        , acceptQ()
        , maxPendConn(maxPendConn)
        , acceptThread()
    {
        if (srvrAddr.getInetAddr().isAny())
            throw INVALID_ARGUMENT("Server's IP address is wildcard");
        if (maxPendConn == 0)
            throw INVALID_ARGUMENT("Size of accept-queue is zero");

        // Because 3 unicast connections per peer-connection
        srvrSock = TcpSrvrSock(srvrAddr, 3*maxPendConn);

        /*
         * Connections are accepted on a separate thread so that a slow connection attempt won't
         * hinder faster attempts.
         */
        LOG_TRACE("Starting thread to accept incoming connections");
        acceptThread = Thread(&PeerConnSrvrImpl::acceptSocks, this);
        // TODO: Lower priority of thread to favor data transmission
    }

    /// Implement when needed
    PeerConnSrvrImpl(const PeerConnSrvrImpl& other) =delete;
    /**
     * Copy assigns.
     * @param[in] rhs  The other instance
     * @return         A reference to this just-assigned instance
     */
    PeerConnSrvrImpl& operator=(const PeerConnSrvrImpl& rhs) =delete;

    ~PeerConnSrvrImpl() noexcept {
        if (acceptThread.joinable()) {
            ::pthread_cancel(acceptThread.native_handle()); // Failsafe
            acceptThread.join();
        }
    }

    /**
     * Returns the socket address of the RPC-server.
     *
     * @return Socket address of RPC-server
     */
    SockAddr getSrvrAddr() const override {
        return srvrSock.getLclAddr();
    }

    /**
     * Returns the next RPC instance.
     *
     * @return              Next RPC instance. Will test false if `halt()` has been called.
     * @throws SystemError  Couldn't accept connection
     */
    PeerConn::Pimpl accept() override {
        Lock lock{mutex};
        cond.wait(lock, [&]{return !acceptQ.empty();});

        auto pImpl = acceptQ.front();

        // Leave "done" sentinel in queue
        if (pImpl)
            acceptQ.pop();

        return pImpl;
    }

    /**
     * Causes `accept()` to return a false object.
     */
    void halt() {
        srvrSock.shutdown();
    }
};

PeerConnSrvr::Pimpl PeerConnSrvr::create(
        const SockAddr& srvrAddr,
        const int       maxPendConn) {
    return Pimpl{new PeerConnSrvrImpl{srvrAddr, maxPendConn}};
}

} // namespace
