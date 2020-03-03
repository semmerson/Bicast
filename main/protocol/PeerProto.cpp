/**
 * Peer-to-peer connection protocol.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PeerProto.cpp
 *  Created on: Nov 5, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "PeerProto.h"
#include "protocol.h"

#include "error.h"
#include "Thread.h"

#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <pthread.h>
#include <thread>

namespace hycast {

class PeerProto::Impl
{
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;
    typedef std::condition_variable Cond;
    typedef std::exception_ptr      ExceptPtr;
    typedef std::atomic_flag        AtomicFlag;
    typedef uint16_t                MsgIdType;

    AtomicFlag     executing;
    ExceptPtr      exceptPtr;
    mutable Mutex  doneMutex;
    mutable Cond   doneCond;
    TcpSock        noticeSock;    ///< For exchanging notices
    /**
     * For sending requests and receiving chunks. Won't exist if the local peer
     * is the source of data-products.
     */
    TcpSock        clntSock;
    /**
     * For receiving requests and sending chunks. Won't exist if the remote
     * peer is the source of data-product.
     */
    TcpSock        srvrSock;
    std::string    string;        ///< `to_string()` string
    bool           srvrSide;      ///< Server-side construction?
    /// Pool of potential ports for temporary servers
    PortPool       portPool;
    PeerProtoObs&  observer;      ///< Observer of this instance
    bool           haltRequested; ///< Halt requested?
    std::thread    noticeThread;  ///< Sends notices to remote peer
    std::thread    serverThread;  ///< Receives requests and sends chunks
    std::thread    clientThread;  ///< Sends requests and receives chunks
    Flags          rmtFlags;      ///< Flags for remote peer

    static const unsigned char PROTO_VERSION = 1;
    static const MsgIdType     PROD_INFO_NOTICE = MsgId::PROD_INFO_NOTICE;
    static const MsgIdType     DATA_SEG_NOTICE = MsgId::DATA_SEG_NOTICE;
    static const MsgIdType     PROD_INFO_REQUEST = MsgId::PROD_INFO_REQUEST;
    static const MsgIdType     DATA_SEG_REQUEST = MsgId::DATA_SEG_REQUEST;
    static const MsgIdType     PROD_INFO = MsgId::PROD_INFO;
    static const MsgIdType     DATA_SEG = MsgId::DATA_SEG;
    static const MsgIdType     PATH_TO_SRC = MsgId::PATH_TO_SRC;
    static const MsgIdType     NO_PATH_TO_SRC = MsgId::NO_PATH_TO_SRC;

    void setString()
    {
        string = "{remote: {addr: " +
                noticeSock.getRmtAddr().getInetAddr().to_string() +
                ", ports: [" + std::to_string(noticeSock.getRmtPort());
        if (clntSock)
            string += ", " + std::to_string(clntSock.getRmtPort());
        if (srvrSock)
            string += ", " + std::to_string(srvrSock.getRmtPort());
        string += "]}, local: {addr: " +
                noticeSock.getLclAddr().getInetAddr().to_string() +
                ", ports: [" + std::to_string(noticeSock.getLclPort());
        if (clntSock)
                string += ", " + std::to_string(clntSock.getLclPort());
        if (srvrSock)
            string += ", " + std::to_string(srvrSock.getLclPort());
        string += "]}}";
    }

    void handleException(const ExceptPtr& exPtr)
    {
        Guard guard(doneMutex);

        if (!exceptPtr) {
            exceptPtr = exPtr;
            doneCond.notify_one();
        }
    }

    ProdIndex recvProdIndex()
    {
        ProdIndex::Type index;

        return noticeSock.read(index)
            ? ProdIndex(index)
            : ProdIndex{};
    }

    bool recvProdNotice()
    {
        //LOG_DEBUG("Receiving product-information notice");
        const auto prodIndex = recvProdIndex();
        if (!prodIndex)
            return false;

        int cancelState;
        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            observer.acceptNotice(prodIndex);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

        return true;
    }

    bool recvSegNotice()
    {
        //LOG_DEBUG("Receiving data-segment notice");
        const auto prodIndex = recvProdIndex();
        if (!prodIndex)
            return false;

        ProdSize segOffset;
        if (!noticeSock.read(segOffset))
            return false;

        int   cancelState;
        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            observer.acceptNotice(SegId{prodIndex, segOffset});
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

        return true;
    }

    /**
     * Returns on EOF;
     */
    void recvNotices()
    {
        try {
            //LOG_DEBUG("Receiving notices");

            for (;;) {
                MsgIdType msgId;

                if (!noticeSock.read(msgId)) // Performs network translation
                    break; // EOF

                if (msgId == PATH_TO_SRC) {
                    observer.pathToSrc();
                }
                else if (msgId == NO_PATH_TO_SRC) {
                    observer.noPathToSrc();
                }
                else if (msgId == PROD_INFO_NOTICE) {
                    if (!recvProdNotice())
                        break;
                }
                else if (msgId == DATA_SEG_NOTICE) {
                    if (!recvSegNotice())
                        break;
                }
                else {
                    throw RUNTIME_ERROR("Invalid message ID: " +
                            std::to_string(msgId));
                }
            }

            halt();
        }
        catch (const std::exception& ex) {
            handleException(std::current_exception());
        }
    }

    bool recvProdRequest()
    {
        ProdIndex::Type prodIndex;

        if (!srvrSock.read(prodIndex)) // Performs network translation
            return false;

        int    cancelState;
        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            observer.acceptRequest(prodIndex);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

        return true;
    }

    bool recvSegRequest()
    {
        try {
            ProdIndex::Type prodIndex;

            if (!srvrSock.read(prodIndex)) // Performs network translation
                return false;

            ProdSize  segOffset;

            if (!srvrSock.read(segOffset))
                return false;

            SegId segId{prodIndex, segOffset};
            {
                //Canceler canceler{false}; // Disable thread cancellation
                observer.acceptRequest(segId);
            }

            return true;
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Caught exception \"%s\"", ex.what());
            throw;
        }
        catch (...) {
            LOG_DEBUG("Caught exception ...");
            throw;
        }
    }

    /**
     * Returns on EOF;
     */
    void recvRequests()
    {
        try {
            LOG_DEBUG("Receiving requests");

            for (;;) {
                MsgIdType msgId;

                if (!srvrSock.read(msgId)) // Performs network translation
                    break; // EOF

                if (msgId == PROD_INFO_REQUEST) {
                    if (!recvProdRequest())
                        break;
                }
                else if (msgId == DATA_SEG_REQUEST) {
                    if (!recvSegRequest())
                        break;
                }
                else {
                    throw RUNTIME_ERROR("Invalid message ID: " +
                            std::to_string(msgId));
                }
            }

            halt();
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Caught exception \"%s\"", ex.what());
            handleException(std::current_exception());
        }
        catch (...) {
            LOG_DEBUG("Caught exception ...");
            throw;
        }
    }

    bool recvCommon(
            ProdIndex& prodIndex,
            ProdSize&  prodSize,
            SegSize&   segSize)
    {
        ProdIndex::Type index;

        if (!clntSock.read(segSize) || !clntSock.read(index) ||
                !clntSock.read(prodSize))
            return false;

        prodIndex = ProdIndex(index);

        return true;
    }

    bool recvProdInfo()
    {
        ProdIndex prodIndex;
        ProdSize  prodSize;
        SegSize   nameLen;

        if (!recvCommon(prodIndex, prodSize, nameLen))
            return false;

        char buf[nameLen];

        if (!clntSock.read(buf, nameLen))
            return false;

        std::string name(buf, nameLen);
        ProdInfo    prodInfo{prodIndex, prodSize, name};
        int         cancelState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            observer.accept(prodInfo);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

        return true;
    }

    bool recvDataSeg()
    {
        ProdIndex prodIndex;
        ProdSize  prodSize;
        SegSize   dataLen;
        if (!recvCommon(prodIndex, prodSize, dataLen))
            return false; // EOF

        ProdSize segOffset;
        if (!clntSock.read(segOffset))
            return false; // EOF

        SegId   segId{prodIndex, segOffset};
        SegInfo segInfo{segId, prodSize, dataLen};
        TcpSeg  seg{segInfo, clntSock};
        int     cancelState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            observer.accept(seg);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

        return true;
    }

    /**
     * Returns on EOF;
     */
    void recvChunks()
    {
        try {
            LOG_DEBUG("Receiving chunks");

            for (;;) {
                MsgIdType msgId;

                // The following performs network translation
                if (!clntSock.read(msgId))
                    break; // EOF

                if (msgId == PROD_INFO) {
                    if (!recvProdInfo())
                        break; // EOF
                }
                else if (msgId == DATA_SEG) {
                    if (!recvDataSeg())
                        break; // EOF
                }
                else {
                    throw RUNTIME_ERROR("Invalid message ID: " +
                            std::to_string(msgId));
                }
            }

            halt();
        }
        catch (const std::exception& ex) {
            handleException(std::current_exception());
        }
    }

    void startTasks()
    {
        /*
         * The tasks aren't detached because then they couldn't be canceled
         * (`std::thread::native_handle()` returns 0 for detached threads).
         */
        if (clntSock) {
            //LOG_DEBUG("Creating thread for sending requests and receiving chunks");
            clientThread = std::thread(&Impl::recvChunks, this);
        }

        try {
            //LOG_DEBUG("Creating thread for sending and receiving notices");
            noticeThread = std::thread(&Impl::recvNotices, this);

            try {
                if (srvrSock) {
                    //LOG_DEBUG("Creating thread for receiving requests and sending chunks");
                    serverThread = std::thread(&Impl::recvRequests, this);
                }
            } // `noticeThread` created
            catch (const std::exception& ex) {
                ::pthread_cancel(noticeThread.native_handle());
                throw;
            }
        } // Client thread created
        catch (const std::exception& ex) {
            if (clntSock)
                ::pthread_cancel(clientThread.native_handle());
            throw;
        }
        catch (...) {
            LOG_DEBUG("Caught exception ...");
            throw;
        }
    }

    void waitUntilDone()
    {
        Lock lock(doneMutex);

        while (!haltRequested && !exceptPtr)
            doneCond.wait(lock);
    }

    /**
     * Idempotent.
     */
    void stopTasks()
    {
        int status;

        if (serverThread.joinable()) {
#if 0
            status = ::pthread_cancel(serverThread.native_handle());
            if (status && status != ESRCH)
                LOG_ERROR("Couldn't cancel server-thread: %s",
                        ::strerror(status));
#else
            srvrSock.shutdown();
#endif
            serverThread.join();
        }
        if (noticeThread.joinable()) {
#if 0
            status = ::pthread_cancel(noticeThread.native_handle());
            if (status && status != ESRCH)
                LOG_ERROR("Couldn't cancel notice-thread: %s",
                        ::strerror(status));
#else
            noticeSock.shutdown();
#endif
            noticeThread.join();
        }
        if (clientThread.joinable()) {
            clntSock.shutdown();
            /*
            status = ::pthread_cancel(clientThread.native_handle());
            if (status && status != ESRCH)
                LOG_ERROR("Couldn't cancel client-thread: %s",
                        ::strerror(status));
             */
            clientThread.join();
        }
    }

    /**
     * Disconnects from the remote peer. Idempotent.
     */
    void disconnect()
    {
        try {
            if (srvrSock)
                srvrSock.shutdown();
            if (clntSock)
                clntSock.shutdown();
            noticeSock.shutdown();
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Failure"));
        }
    }

    TcpSrvrSock createSrvrSock(
            InetAddr  inAddr,
            PortPool& portPool,
            const int queueSize)
    {
        const int n = portPool.size();
        int       i = 0;

        while (++i <= n) {
            const in_port_t port = portPool.take();
            SockAddr        srvrAddr(inAddr.getSockAddr(port));

            try {
                return TcpSrvrSock(srvrAddr, queueSize);
            }
            catch (const std::exception& ex) {
                log_error(ex);
                portPool.add(port);
            }
        }

        throw RUNTIME_ERROR("Couldn't create server socket");
    }

    void send(
            const MsgIdType msgId,
            const ProdIndex prodIndex,
            TcpSock&        sock)
    {
        // The following perform network translation
        sock.write(msgId);
        sock.write(prodIndex.getValue());
    }

    void send(
            const MsgIdType msgId,
            const SegId&    id,
            TcpSock&        sock)
    {
        // The following perform network translation
        sock.write(msgId);
        sock.write(id.getProdIndex().getValue());
        sock.write(id.getOffset());
    }

public:
    /**
     * Server-side construction. Remote peer can't be the source.
     *
     * @param[in] sock      `::accept()`ed TCP socket
     * @param[in] portPool  Pool of potential port numbers for temporary servers
     * @param[in] observer  Observer of this instance
     * @param[in] isSource  This instance is source of data-products?
     * @exceptionsafety     Strong guarantee
     * @cancellationpoint   Yes
     */
    Impl(   TcpSock&      sock,
            PortPool&     portPool,
            PeerProtoObs& observer,
            bool          isSource)
        : executing{false}
        , exceptPtr{}
        , doneMutex{}
        , doneCond{}
        , noticeSock(sock)
        , clntSock{}
        , srvrSock{}
        , string{}
        , srvrSide{true}
        , portPool{portPool}
        , observer(observer)
        , haltRequested{false}
        , noticeThread{}
        , serverThread{}
        , clientThread{}
        , rmtFlags{}
    {
        InetAddr rmtInAddr{sock.getRmtAddr().getInetAddr()};
        //LOG_DEBUG("rmtInAddr: %s", rmtInAddr.to_string().c_str());

        noticeSock.setDelay(false);

        // Temporary server for receiving requests and sending chunks
        InetAddr    lclInAddr = sock.getLclAddr().getInetAddr();
        TcpSrvrSock srvrSrvrSock{createSrvrSock(lclInAddr, portPool, 1)};

        try {
            // Temporary server for sending requests and receiving chunks
            TcpSrvrSock clntSrvrSock{};
            if (!isSource)
                clntSrvrSock = TcpSrvrSock(createSrvrSock(lclInAddr, portPool,
                        1));

            try {
                // Inform the remote peer if this instance is the source
                const char yes = isSource;
                noticeSock.write(&yes, 1);

                // Send port numbers of temporary server sockets to remote peer
                noticeSock.write(srvrSrvrSock.getLclPort());
                if (clntSrvrSock)
                    noticeSock.write(clntSrvrSock.getLclPort());

                // Accept sockets from temporary servers
                // TODO: Ensure connections are from remote peer
                srvrSock = TcpSock{srvrSrvrSock.accept()};
                srvrSock.setDelay(true); // Consolidate ACK and chunk
                if (clntSrvrSock) {
                    clntSock = TcpSock{clntSrvrSock.accept()};
                    clntSock.setDelay(false); // Requests are immediately sent
                }

                setString();
            }
            catch (...) {
                if (clntSrvrSock)
                    portPool.add(clntSrvrSock.getLclPort());
                std::throw_with_nested(RUNTIME_ERROR(
                        std::string("Couldn't start ") +
                        (isSource ? "source-" : "non-source ") +
                        "server on socket " + sock.to_string()));
            }
        }
        catch (...) {
            portPool.add(srvrSrvrSock.getLclPort());
            throw;
        }
    }

    /**
     * Client-side construction. Can't be the source of data-products.
     *
     * @param[in] rmtSrvrAddr         Socket address of the remote peer-server
     * @param[in] observer            Observer of this instance
     * @throws    std::system_error   System error
     * @throws    std::runtime_error  Remote peer closed the connection
     * @exceptionsafety               Strong guarantee
     * @cancellationpoint             Yes
     */
    Impl(   const SockAddr& rmtSrvrAddr,
            PeerProtoObs&   observer)
        : executing{false}
        , exceptPtr{}
        , doneMutex{}
        , doneCond{}
        , noticeSock{TcpClntSock(rmtSrvrAddr)}
        , clntSock{}
        , srvrSock{}
        , string()
        , srvrSide{false}
        , portPool{}
        , observer(observer)
        , haltRequested{false}
        , noticeThread{}
        , serverThread{}
        , clientThread{}
        , rmtFlags{}
    {
        noticeSock.setDelay(false);

        // Read if the remote peer is the source of data-products
        char rmtIsSource;
        if (!noticeSock.read(&rmtIsSource, 1))
            throw RUNTIME_ERROR("EOF");

        if (rmtIsSource)
            rmtFlags.setPathToSrc();

        // Read port numbers of remote peer's temporary servers
        in_port_t rmtSrvrPort, rmtClntPort;
        if (!noticeSock.read(rmtSrvrPort))
            throw RUNTIME_ERROR("EOF");
        if (!rmtIsSource && !noticeSock.read(rmtClntPort))
            throw RUNTIME_ERROR("EOF");

        // Create sockets to remote peer's temporary servers
        clntSock = TcpClntSock(rmtSrvrAddr.clone(rmtSrvrPort));
        if (!rmtIsSource)
            srvrSock = TcpClntSock(rmtSrvrAddr.clone(rmtClntPort));

        clntSock.setDelay(false);
        if (srvrSock)
            srvrSock.setDelay(true); // Consolidate ACK and chunk

        setString();
    }

    ~Impl() noexcept
    {
        try {
            stopTasks(); // Idempotent
            disconnect(); // Idempotent
            if (srvrSide) {
                if (clntSock)
                    portPool.add(clntSock.getLclPort());
                portPool.add(srvrSock.getLclPort());
            }
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex, "Failure");
        }
    }

    SockAddr getRmtAddr() const
    {
        return noticeSock.getRmtAddr();
    }

    SockAddr getLclAddr() const
    {
        return noticeSock.getLclAddr();
    }

    std::string to_string() const
    {
        return "{rmtAddr: " + getRmtAddr().to_string() + ", lclAddr: " +
                getLclAddr().to_string() + "}";
    }

    /**
     * Executes this instance by starting subtasks. Doesn't return until one of
     * the following:
     *   - The remote peer closes the connection;
     *   - `halt()` is called;
     *   - An exception is thrown by a subtask.
     * Upon return, all subtasks have terminated and the remote peer will have
     * been disconnected. If `halt()` is called before this method, then this
     * instance will return immediately and won't execute.
     *
     * @throws    std::system_error   System error
     * @throws    std::runtime_error  Remote peer closed the connection
     * @throws    std::logic_error    This method has already been called
     */
    void operator ()()
    {
        try {
            if (executing.test_and_set())
                throw LOGIC_ERROR("Already called");

            startTasks();

            try {
                waitUntilDone();
                stopTasks(); // Idempotent
                disconnect(); // Idempotent

                Guard guard{doneMutex};
                if (!haltRequested && exceptPtr)
                    std::rethrow_exception(exceptPtr);
            }
            catch (...) {
                stopTasks(); // Idempotent
                disconnect(); // Idempotent
                throw;
            }
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Failure"));
        }
    }

    /**
     * Halts execution. Does nothing if `operator()()` has not been called;
     * otherwise, causes `operator()()` to return. Idempotent.
     *
     * @cancellationpoint No
     */
    void halt() noexcept
    {
        Guard guard(doneMutex);

        haltRequested = true;
        doneCond.notify_one();
    }

    void notify(const ProdIndex prodIndex)
    {
        if (srvrSock)
            send(PROD_INFO_NOTICE, prodIndex, noticeSock);
    }

    void notify(const SegId& id)
    {
        if (srvrSock)
            send(DATA_SEG_NOTICE, id, noticeSock);
    }

    void request(const ProdIndex prodIndex)
    {
        if (clntSock)
            send(PROD_INFO_REQUEST, prodIndex, clntSock);
    }

    void request(const SegId segId)
    {
        if (clntSock)
            send(DATA_SEG_REQUEST, segId, clntSock);
    }

    void send(const ProdInfo& info)
    {
        if (srvrSock) {
            const std::string& name = info.getProdName();
            const SegSize      nameLen = name.length();

            LOG_DEBUG("Sending product-information %s", info.to_string().data());

            // The following perform host-to-network translation
            srvrSock.write(PROD_INFO);
            srvrSock.write(nameLen);
            srvrSock.write(info.getProdIndex().getValue());
            srvrSock.write(info.getProdSize());

            srvrSock.write(name.data(), nameLen); // No network translation
        }
    }

    void send(const MemSeg& seg)
    {
        if (srvrSock) {
            try {
                const SegInfo& info = seg.getSegInfo();
                SegSize        segSize = info.getSegSize();

                LOG_DEBUG("Sending segment %s", info.to_string().data());

                // The following perform host-to-network translation
                srvrSock.write(DATA_SEG);
                srvrSock.write(segSize);
                srvrSock.write(info.getId().getProdIndex().getValue());
                srvrSock.write(info.getProdSize());
                srvrSock.write(info.getId().getOffset());

                // No network translation
                srvrSock.write(seg.data(), segSize);
            }
            catch (const std::exception& ex) {
                LOG_DEBUG("Caught exception \"%s\"", ex.what());
                throw;
            }
            catch (...) {
                LOG_DEBUG("Caught exception ...");
                throw;
            }
        }
    }
};

PeerProto::PeerProto(Impl* impl)
    : pImpl{impl}
{}

PeerProto::PeerProto(
        TcpSock&      sock,
        PortPool&     portPool,
        PeerProtoObs& observer,
        const bool    isSource)
    : PeerProto{new Impl(sock, portPool, observer, isSource)}
{}

PeerProto::PeerProto(
        const SockAddr& rmtSrvrAddr,
        PeerProtoObs&   observer)
    : PeerProto{new Impl(rmtSrvrAddr, observer)}
{}

PeerProto::operator bool() const
{
    return pImpl.operator bool();
}

SockAddr PeerProto::getRmtAddr() const
{
    return pImpl->getRmtAddr();
}

SockAddr PeerProto::getLclAddr() const
{
    return pImpl->getLclAddr();
}

std::string PeerProto::to_string() const
{
    return pImpl->to_string();
}

void PeerProto::operator()() const
{
    pImpl->operator()();
}

void PeerProto::halt() const
{
    pImpl->halt();
}

void PeerProto::notify(const ProdIndex prodId) const
{
    pImpl->notify(prodId);
}

void PeerProto::notify(const SegId& id) const
{
    pImpl->notify(id);
}

void PeerProto::request(const ProdIndex prodId) const
{
    pImpl->request(prodId);
}

void PeerProto::request(const SegId segId) const
{
    pImpl->request(segId);
}

void PeerProto::send(const ProdInfo& prodInfo) const
{
    pImpl->send(prodInfo);
}

void PeerProto::send(const MemSeg& memSeg) const
{
    pImpl->send(memSeg);
}

} // namespace
