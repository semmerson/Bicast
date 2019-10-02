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

#include "Codec.h"
#include "error.h"
#include "PeerConn.h"
#include "PortPool.h"

namespace hycast {

class PeerConn::Impl
{
public:
    virtual ~Impl() =0;

    virtual const SockAddr& getRmtAddr() const noexcept =0;

    virtual const SockAddr& getLclAddr() const noexcept =0;

    virtual std::string to_string() const noexcept =0;

    virtual void notify(const ChunkId& notice) =0;

    virtual ChunkId getNotice() =0;

    virtual void request(const ChunkId& request) =0;

    virtual ChunkId getRequest() =0;

    virtual void send(const MemChunk& chunk) =0;

    virtual StreamChunk getChunk() =0;

    virtual void disconnect() =0;
};

PeerConn::Impl::~Impl()
{}

/******************************************************************************/

/**
 * A connection between peers that uses three `Wire`s to minimize latency.
 */
class PeerConn3 final : public PeerConn::Impl
{
private:
    SockAddr    rmtSockAddr; ///< Remote socket address
    SockAddr    lclSockAddr; ///< Local socket address
    StreamCodec noticeCodec;   ///< Connection for exchanging notices
    StreamCodec clntCodec;     ///< Connection for sending requests and receiving chunks
    StreamCodec srvrCodec;     ///< Connection for receiving requests and sending chunks
    std::string string;      ///< `to_string()` string

    SrvrSock createSrvrSock(
            InAddr    inAddr,
            PortPool& portPool,
            const int queueSize)
    {
        const int n = portPool.size();
        int       i = 0;

        while (++i <= n) {
            const in_port_t port = portPool.take();
            SockAddr        srvrAddr(inAddr.getSockAddr(port));

            try {
                return SrvrSock(srvrAddr, queueSize);
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
     * @param[in] sock      `::accept()`ed socket
     */
    PeerConn3(Socket&  sock)
        : rmtSockAddr(sock.getPeerAddr())
        , lclSockAddr{sock.getAddr()}
        , noticeCodec(sock)
        , clntCodec{}
        , srvrCodec{}
        , string{}
    {
        InAddr rmtInAddr{rmtSockAddr.getInAddr()};
        LOG_DEBUG("rmtInAddr: %s", rmtInAddr.to_string().c_str());

        sock.setDelay(false);

        // Read port numbers of client's temporary servers
        in_port_t rmtSrvrPort, rmtClntPort;
        noticeCodec.decode(rmtSrvrPort);
        noticeCodec.decode(rmtClntPort);

        // Create sockets for requesting and receiving chunks
        ClntSock clntSock(rmtSockAddr.clone(rmtSrvrPort));
        ClntSock srvrSock(rmtSockAddr.clone(rmtClntPort));

        clntSock.setDelay(false);
        srvrSock.setDelay(true); // Consolidate ACK and chunk

        // Create support for requesting and receiving chunks
        clntCodec = StreamCodec(clntSock);
        srvrCodec = StreamCodec(srvrSock);

        string = "{remote: {addr: " +
                sock.getPeerAddr().getInAddr().to_string() +
                ", ports: [" + std::to_string(sock.getPeerPort()) +
                ", " + std::to_string(clntSock.getPeerPort()) +
                ", " + std::to_string(srvrSock.getPeerPort()) +
                "]}, local: {addr: " +
                sock.getAddr().getInAddr().to_string() +
                ", ports: [" + std::to_string(sock.getPort()) +
                ", " + std::to_string(clntSock.getPort()) +
                ", " + std::to_string(srvrSock.getPort()) +
                "]}}";
    }

    /**
     * Client-side construction.
     *
     * @param[in] rmtSrvrAddr         Socket address of the remote server
     * @param[in] portPool            Pool of potential port numbers for
     *                                temporary servers
     * @throws    std::system_error   System error
     * @throws    std::runtime_error  Remote peer closed the connection
     * @cancellationpoint             Yes
     */
    PeerConn3(
            const SockAddr& rmtSrvrAddr,
            PortPool&       portPool)
        : rmtSockAddr{rmtSrvrAddr}
        , lclSockAddr{}
        , noticeCodec{}
        , clntCodec{}
        , srvrCodec{}
    {
        // Create socket for exchanging notices
        ClntSock noticeSock{rmtSrvrAddr};
        noticeSock.setDelay(false);

        // Create support for exchanging notices
        lclSockAddr = noticeSock.getAddr();
        noticeCodec = StreamCodec{noticeSock};

        // Create temporary server sockets
        InAddr   lclInAddr = lclSockAddr.getInAddr();
        SrvrSock srvrSrvrSock{createSrvrSock(lclInAddr, portPool, 1)};
        SrvrSock clntSrvrSock{createSrvrSock(lclInAddr, portPool, 1)};

        // Send port numbers of temporary servers to remote peer
        noticeCodec.encode(srvrSrvrSock.getPort());
        noticeCodec.encode(clntSrvrSock.getPort());

        // Accept sockets for requesting and receiving chunks
        Socket srvrSock{srvrSrvrSock.accept()};
        Socket clntSock{clntSrvrSock.accept()};

        clntSock.setDelay(false);
        srvrSock.setDelay(true); // Consolidate ACK and chunk

        // Create wires for requesting and receiving chunks
        clntCodec = StreamCodec{clntSock};
        srvrCodec = StreamCodec{srvrSock};

        string = "{remote: {addr: " +
                noticeSock.getPeerAddr().getInAddr().to_string() +
                ", ports: [" + std::to_string(noticeSock.getPeerPort()) +
                ", " + std::to_string(clntSock.getPeerPort()) +
                ", " + std::to_string(srvrSock.getPeerPort()) +
                "]}, local: {addr: " +
                noticeSock.getAddr().getInAddr().to_string() +
                ", ports: [" + std::to_string(noticeSock.getPort()) +
                ", " + std::to_string(clntSock.getPort()) +
                ", " + std::to_string(srvrSock.getPort()) +
                "]}}";
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
        noticeCodec.encode(notice);
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
        ChunkId id;
        noticeCodec.decode(id);
        return id;
    }

    void request(const ChunkId& request)
    {
        clntCodec.encode(request);
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
        ChunkId id;
        srvrCodec.decode(id);
        return id;
    }

    void send(const MemChunk& chunk)
    {
        srvrCodec.encode(chunk);
    }

    /**
     * Reads a chunk of data.
     *
     * @return                     Latent chunk of data
     * @throws std::system_error   System error
     * @throws std::runtime_error  Remote peer closed the connection
     */
    StreamChunk getChunk()
    {
        StreamChunk chunk;
        clntCodec.decode(chunk);
        return chunk;
    }

    /**
     * Disconnects from the remote peer. Idempotent.
     */
    void disconnect()
    {
        srvrCodec = StreamCodec();
        clntCodec = StreamCodec();
        noticeCodec = StreamCodec();
    }
};

/******************************************************************************/

PeerConn::PeerConn(Impl* const impl)
    : pImpl{impl}
{}

PeerConn::PeerConn(
        const SockAddr& srvrAddr,
        PortPool&       portPool)
    : PeerConn{new PeerConn3(srvrAddr, portPool)}
{}

PeerConn::PeerConn(Socket&   sock)
    : PeerConn{new PeerConn3(sock)}
{}

PeerConn::PeerConn(Socket&& sock)
    : PeerConn{new PeerConn3(sock)}
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

StreamChunk PeerConn::getChunk()
{
    return pImpl->getChunk();
}

void PeerConn::disconnect()
{
    pImpl->disconnect();
}

} // namespace
