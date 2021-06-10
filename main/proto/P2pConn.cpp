/**
 * This file defines classes related to a Hycast protocol connection.
 *
 *  @file:  P2pConn.cpp
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

#include "error.h"
#include "P2pConn.h"

#include <list>
#include <queue>
#include <unordered_map>

namespace hycast {

/******************************************************************************/

/**
 * Implementation of a Hycast connection.
 */
class P2pConn::Impl
{
    static inline void write(TcpSock& sock, PduId id) {
        sock.write(static_cast<PduType>(id));
    }

    static inline void write(TcpSock& sock, const ProdIndex index) {
        sock.write((ProdIndex::Type)index);
    }

    static inline void write(TcpSock& sock, const DataSegId& id) {
        write(sock, id.prodIndex);
        sock.write(id.offset);
    }

    static inline void write(TcpSock& sock, const ProdInfo& prodInfo) {
        write(sock, prodInfo.index);
        sock.write(prodInfo.name);
        sock.write(prodInfo.size);
        sock.write(prodInfo.created.sec);
        sock.write(prodInfo.created.nsec);
    }

    static inline void write(TcpSock& sock, const DataSeg& dataSeg) {
        write(sock, dataSeg.segId());
        sock.write(dataSeg.prodSize());
        sock.write(dataSeg.data(), dataSeg.size());
    }

protected:
    /*
     * If a single socket is used for asynchronous communication and reading and
     * writing occur on the same thread, then deadlock will occur if both
     * receive buffers are full and each end is trying to write. To prevent
     * this, three sockets are used and a thread that reads from one socket will
     * write to another.
     */
    TcpSock noticeSock;
    TcpSock requestSock;
    TcpSock dataSock;

    /**
     * Reorders the sockets so that the notice socket has the lowest
     * client-side port number, then the request socket, and then the data
     * socket.
     */
    virtual void reorder() noexcept =0;

public:
    /**
     * Server-side construction.
     */
    Impl() =default;

    /**
     * Client-side construction.
     *
     * @param[in] srvrAddr  Socket address of Hycast server
     */
    Impl(const SockAddr srvrAddr)
        : noticeSock(TcpClntSock(srvrAddr))
        , requestSock(TcpClntSock(srvrAddr))
        , dataSock(TcpClntSock(srvrAddr))
    {
        // Keep constructor consonant with `P2pSrvr::accept()`
        const in_port_t noticePort = noticeSock.getLclPort();

        noticeSock.write(noticePort);
        requestSock.write(noticePort);
        dataSock.write(noticePort);
    }

    virtual ~Impl() {
    }

    /**
     * Sets the next, individual connection.
     *
     * @param[in] sock        Relevant socket
     * @retval    `false`     P2P connection isn't complete
     * @retval    `true`      P2P connection is complete
     * @throw     LogicError  Connection is already complete
     */
    virtual bool set(TcpSock sock) =0;

    operator bool() noexcept {
        return static_cast<bool>(dataSock);
    }

    /**
     * Notifies the remote peer.
     */
    void notify(const PubPath notice) {
        write(noticeSock, PduId::PUB_PATH_NOTICE);
        noticeSock.write(notice.operator bool());
    }
    void notify(const ProdIndex notice) {
        write(noticeSock, PduId::PROD_INFO_NOTICE);
        write(noticeSock, notice);
    }
    void notify(const DataSegId& notice) {
        write(noticeSock, PduId::DATA_SEG_NOTICE);
        write(noticeSock, notice);
    }

    /**
     * Requests data from the remote peer.
     */
    void request(const ProdIndex request) {
        write(requestSock, PduId::PROD_INFO_NOTICE);
        write(requestSock, request);
    }
    void request(const DataSegId& request) {
        write(requestSock, PduId::DATA_SEG_NOTICE);
        write(requestSock, request);
    }

    /**
     * Sends data to the remote peer.
     */
    void send(const ProdInfo& data) {
        write(dataSock, PduId::PROD_INFO);
        write(dataSock, data);
    }
    void send(const DataSeg& data) {
        write(dataSock, PduId::DATA_SEG);
        write(dataSock, data);
    }
};

P2pConn::operator bool() noexcept {
    return pImpl && *pImpl;
}

void P2pConn::notify(const PubPath notice) {
    pImpl->notify(notice);
}
void P2pConn::notify(const ProdIndex notice) {
    pImpl->notify(notice);
}
void P2pConn::notify(const DataSegId& notice) {
    pImpl->notify(notice);
}

void P2pConn::request(const ProdIndex request) {
    pImpl->request(request);
}
void P2pConn::request(const DataSegId& request) {
    pImpl->request(request);
}

void P2pConn::send(const ProdInfo& data) {
    pImpl->send(data);
}
void P2pConn::send(const DataSeg& data) {
    pImpl->send(data);
}

/******************************************************************************/

/**
 * Client-side P2P connection.
 */
class ClntP2pConn final : public P2pConn::Impl
{
protected:
    void reorder() noexcept {
        if (requestSock.getLclPort() < noticeSock.getLclPort())
            requestSock.swap(noticeSock);

        if (dataSock.getLclPort() < requestSock.getLclPort()) {
            dataSock.swap(requestSock);
            if (requestSock.getLclPort() < noticeSock.getLclPort())
                requestSock.swap(noticeSock);
        }
    }

public:
    /**
     * Constructs.
     *
     * @param[in] srvrAddr  Socket address of Hycast server
     */
    ClntP2pConn(const SockAddr srvrAddr)
        : Impl(srvrAddr)
    {
        reorder();
    }

    bool set(TcpSock sock) {
        throw LOGIC_ERROR("Client-side P2P connection is complete");
    }
};

P2pConn::P2pConn(SockAddr srvrAddr)
    : pImpl(std::make_shared<ClntP2pConn>(srvrAddr))
{}

/******************************************************************************/

/**
 * Server-side P2P connection.
 */
class SrvrP2pConn final : public P2pConn::Impl
{
protected:
    void reorder() noexcept {
        if (requestSock.getRmtPort() < noticeSock.getRmtPort())
            requestSock.swap(noticeSock);

        if (dataSock.getRmtPort() < requestSock.getRmtPort()) {
            dataSock.swap(requestSock);
            if (requestSock.getRmtPort() < noticeSock.getRmtPort())
                requestSock.swap(noticeSock);
        }
    }

public:
    /**
     * Constructs.
     *
     * @param[in] srvrAddr  Socket address of Hycast server
     */
    SrvrP2pConn()
        : Impl()
    {}

    bool set(TcpSock sock) {
        // NB: Keep function consonant with `Impl(SockAddr)`
        bool isComplete = false;

        if (!noticeSock) {
            noticeSock = sock;
        }
        else if (!requestSock) {
            requestSock = sock;
        }
        else if (!dataSock) {
            dataSock = sock;
            reorder();
            isComplete = true;
        }
        else {
            throw LOGIC_ERROR("Server-side P2P connection is complete");
        }

        return isComplete;
    }
};

P2pConn::P2pConn()
    : pImpl(std::make_shared<SrvrP2pConn>())
{}

bool P2pConn::set(TcpSock sock) {
    return pImpl->set(sock);
}

/******************************************************************************/

/**
 * P2P server implementation.
 */
class P2pSrvr::Impl
{
    class P2pConnFactory
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

        using Map = std::unordered_map<SockAddr, P2pConn, Hash, AreEqual>;

        Map p2pConns;

    public:
        P2pConnFactory()
            : p2pConns()
        {}

        /**
         * Adds an individual socket to a P2P connection.
         *
         * @param[in] sock        Individual socket
         * @return                Corresponding P2P connection. Will test false
         *                        iff the P2P connection isn't yet complete.
         */
        P2pConn add(TcpSock sock, in_port_t noticePort) {
            // TODO: Limit the number of outstanding connections
            // TODO: Purge old entries
            auto key = sock.getRmtAddr().clone(noticePort);
            auto p2pConn = p2pConns[key];

            if (p2pConn.set(sock))
                p2pConns.erase(key);

            return p2pConn;
        }
    };

    using P2pConnQ = std::queue<P2pConn, std::list<P2pConn>>;

    Mutex               mutex;
    Cond                cond;
    P2pConnFactory      p2pConnFactory;
    const SockAddr      srvrAddr;
    const TcpSrvrSock   srvrSock;
    Thread              acceptThread;
    P2pConnQ            acceptQ;
    P2pConnQ::size_type maxAccept;

    void acceptSock(TcpSock sock) {
        in_port_t noticePort;

        if (sock.read(noticePort)) { // Might take a while
            // The rest is fast
            Guard guard{mutex};

            if (acceptQ.size() < maxAccept) {
                auto p2pConn = p2pConnFactory.add(sock, noticePort);

                if (p2pConn) {
                    acceptQ.push(p2pConn);
                    cond.notify_one();
                }
            }
        }
    }

    void runAccept() {
        // Accept on new thread to support concurrent connection attempts
        try {
            // TODO: Limit the number of threads
            // TODO: Lower the priority of the thread
            for (;;)
                Thread(&Impl::acceptSock, this, srvrSock.accept()).detach();
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
        }
    }

public:
    /**
     * Constructs from the local address of the server.
     *
     * @param[in] srvrAddr   Local Address of P2P server
     * @param[in] maxAccept  Maximum number of outstanding P2P connections
     */
    Impl(const SockAddr& srvrAddr, const unsigned  maxAccept)
        : mutex()
        , cond()
        , srvrAddr(srvrAddr)
        , srvrSock(srvrAddr)
        , acceptThread(&Impl::runAccept, this)
        , acceptQ()
        , maxAccept(maxAccept)
    {}

    ~Impl() noexcept {
        ::pthread_cancel(acceptThread.native_handle());
        if (acceptThread.joinable())
            acceptThread.join();
    }

    /**
     * Returns the next, accepted, peer-to-peer connection.
     *
     * @return Next P2P connection
     */
    P2pConn accept() {
        Lock lock{mutex};

        while (acceptQ.empty())
            cond.wait(lock);

        auto p2pConn = acceptQ.front();
        acceptQ.pop();

        return p2pConn;
    }
};

P2pSrvr::P2pSrvr(const SockAddr& srvrAddr,
                 const unsigned  maxAccept)
    : pImpl(std::make_shared<Impl>(srvrAddr, maxAccept))
{}

P2pConn P2pSrvr::accept() {
    return pImpl->accept();
}

} // namespace
