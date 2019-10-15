/**
 * Connection between peers.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PeerConn.cpp
 *  Created on: May 10, 2019
 *      Author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "PeerConn.h"
#include "PortPool.h"
#include "Socket.h"

namespace hycast {

/**
 * A connection between peers that uses three TCP sockets to minimize latency.
 */
class PeerConn::Impl
{
private:
    SockAddr    rmtSockAddr; ///< Remote socket address
    TcpSock     noticeSock;  ///< For exchanging notices
    TcpSock     clntSock;    ///< For sending requests and receiving chunks
    TcpSock     srvrSock;    ///< For receiving requests and sending chunks
    SockAddr    lclSockAddr; ///< Local socket address
    std::string string;      ///< `to_string()` string
    bool        srvrSide;    ///< Server-side construction?
    PortPool    portPool;    ///< Pool of potential ports for temporary servers

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

public:
    /**
     * Server-side construction.
     *
     * @param[in] sock      `::accept()`ed TCP socket
     * @param[in] portPool  Pool of potential port numbers for temporary servers
     * @exceptionsafety     Strong guarantee
     * @cancellationpoint   Yes
     */
    Impl(   TcpSock&  sock,
            PortPool& portPool)
        : rmtSockAddr(sock.getRmtAddr())
        , noticeSock(sock)
        , clntSock{}
        , srvrSock{}
        , lclSockAddr{sock.getLclAddr()}
        , string{}
        , srvrSide{true}
        , portPool{portPool}
    {
        InetAddr rmtInAddr{rmtSockAddr.getInAddr()};
        LOG_DEBUG("rmtInAddr: %s", rmtInAddr.to_string().c_str());

        noticeSock.setDelay(false);

        // Create temporary server sockets
        InetAddr    lclInAddr = lclSockAddr.getInAddr();
        TcpSrvrSock srvrSrvrSock{createSrvrSock(lclInAddr, portPool, 1)};

        try {
            TcpSrvrSock clntSrvrSock{createSrvrSock(lclInAddr, portPool, 1)};

            try {
                // Send port numbers of temporary server sockets to remote peer
                noticeSock.write(srvrSrvrSock.getLclPort());
                noticeSock.write(clntSrvrSock.getLclPort());

                // Accept sockets from temporary servers
                // TODO: Ensure connections are from remote peer
                srvrSock = TcpSock{srvrSrvrSock.accept()};
                clntSock = TcpSock{clntSrvrSock.accept()};

                clntSock.setDelay(false);
                srvrSock.setDelay(true); // Consolidate ACK and chunk

                string = "{remote: {addr: " +
                        sock.getRmtAddr().getInAddr().to_string() +
                        ", ports: [" + std::to_string(sock.getRmtPort()) +
                        ", " + std::to_string(clntSock.getRmtPort()) +
                        ", " + std::to_string(srvrSock.getRmtPort()) +
                        "]}, local: {addr: " +
                        sock.getLclAddr().getInAddr().to_string() +
                        ", ports: [" + std::to_string(sock.getLclPort()) +
                        ", " + std::to_string(clntSock.getLclPort()) +
                        ", " + std::to_string(srvrSock.getLclPort()) +
                        "]}}";
            }
            catch (...) {
                portPool.add(clntSrvrSock.getLclPort());
                throw;
            }
        }
        catch (...) {
            portPool.add(srvrSrvrSock.getLclPort());
            throw;
        }
    }

    /**
     * Client-side construction.
     *
     * @param[in] rmtSrvrAddr         Socket address of the remote server
     * @throws    std::system_error   System error
     * @throws    std::runtime_error  Remote peer closed the connection
     * @exceptionsafety               Strong guarantee
     * @cancellationpoint             Yes
     */
    Impl(const SockAddr& rmtSrvrAddr)
        : rmtSockAddr{rmtSrvrAddr}
        , noticeSock{TcpClntSock(rmtSrvrAddr)}
        , clntSock{}
        , srvrSock{}
        , lclSockAddr{noticeSock.getLclAddr()}
        , string()
        , srvrSide{false}
    {
        noticeSock.setDelay(false);

        // Read port numbers of remote peer's temporary servers
        in_port_t rmtSrvrPort, rmtClntPort;
        noticeSock.read(rmtSrvrPort);
        noticeSock.read(rmtClntPort);

        // Create sockets to remote peer's temporary servers
        clntSock = TcpClntSock(rmtSockAddr.clone(rmtSrvrPort));
        srvrSock = TcpClntSock(rmtSockAddr.clone(rmtClntPort));

        clntSock.setDelay(false);
        srvrSock.setDelay(true); // Consolidate ACK and chunk

        string = "{remote: {addr: " +
                noticeSock.getRmtAddr().getInAddr().to_string() +
                ", ports: [" + std::to_string(noticeSock.getRmtPort()) +
                ", " + std::to_string(clntSock.getRmtPort()) +
                ", " + std::to_string(srvrSock.getRmtPort()) +
                "]}, local: {addr: " +
                noticeSock.getLclAddr().getInAddr().to_string() +
                ", ports: [" + std::to_string(noticeSock.getLclPort()) +
                ", " + std::to_string(clntSock.getLclPort()) +
                ", " + std::to_string(srvrSock.getLclPort()) +
                "]}}";
    }

    ~Impl()
    {
        if (srvrSide) {
            portPool.add(clntSock.getLclPort());
            portPool.add(srvrSock.getLclPort());
        }
    }

    /**
     * Returns the socket address of the remote peer. On the client-side, this
     * will be the address of the peer-server; on the server-side, this will be
     * the address of the `accept()`ed socket.
     *
     * @return Socket address of the remote peer.
     */
    const SockAddr& getRmtAddr() const noexcept {
        return rmtSockAddr;
    }

    /**
     * Returns the local socket address.
     *
     * @return Local Socket address
     */
    const SockAddr& getLclAddr() const noexcept {
        return lclSockAddr;
    }

    std::string to_string() const noexcept
    {
        return string;
    }

    void notify(const ChunkId& notice)
    {
        notice.write(noticeSock);
    }

    /**
     * Reads a notice
     *
     * @return                     Chunk ID
     * @throws std::system_error   System error
     * @throws std::runtime_error  Remote peer closed the connection
     */
    ChunkId getNotice()
    {
        return ChunkId::read(noticeSock);
    }

    void request(const ChunkId& request)
    {
        request.write(clntSock);
    }

    /**
     * Reads a request.
     *
     * @return                     ID of requested chunk
     * @throws std::system_error   System error
     * @throws std::runtime_error  Remote peer closed the connection
     */
    ChunkId getRequest()
    {
        return ChunkId::read(srvrSock);
    }

    void send(const MemChunk& chunk)
    {
        chunk.write(srvrSock);
    }

    /**
     * Reads a chunk of data.
     *
     * @return                     Latent chunk of data
     * @throws std::system_error   System error
     * @throws std::runtime_error  Remote peer closed the connection
     */
    TcpChunk getChunk()
    {
        return TcpChunk{clntSock};
    }

    /**
     * Disconnects from the remote peer. Idempotent.
     */
    void disconnect()
    {
        srvrSock.shutdown();
        clntSock.shutdown();
        noticeSock.shutdown();
    }
};

/******************************************************************************/

PeerConn::PeerConn(Impl* const impl)
    : pImpl{impl}
{}

PeerConn::PeerConn(const SockAddr& srvrAddr)
    : PeerConn{new Impl(srvrAddr)}
{}

PeerConn::PeerConn(
        TcpSock&  sock,
        PortPool& portPool)
    : PeerConn{new Impl(sock, portPool)}
{}

PeerConn::PeerConn(
        TcpSock&& sock,
        PortPool& portPool)
    : PeerConn{new Impl(sock, portPool)}
{}

const SockAddr& PeerConn::getRmtAddr() const noexcept
{
    return pImpl->getRmtAddr();
}

const SockAddr& PeerConn::getLclAddr() const noexcept
{
    return pImpl->getLclAddr();
}

std::string PeerConn::to_string() const noexcept
{
    return pImpl->to_string();
}

void PeerConn::notify(const ChunkId& notice)
{
    pImpl->notify(notice);
}

ChunkId PeerConn::getNotice() {
    return pImpl->getNotice();
}

void PeerConn::request(const ChunkId& request)
{
    pImpl->request(request);
}

ChunkId PeerConn::getRequest()
{
    return pImpl->getRequest();
}

void PeerConn::send(const MemChunk& chunk)
{
    pImpl->send(chunk);
}

TcpChunk PeerConn::getChunk()
{
    return pImpl->getChunk();
}

void PeerConn::disconnect()
{
    pImpl->disconnect();
}

} // namespace
