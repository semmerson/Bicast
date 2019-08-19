/**
 * Proxy for a remote peer.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: RemotePeer.cpp
 *  Created on: May 10, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "InAddr.h"
#include "PortPool.h"
#include "RemotePeer.h"

namespace hycast {

class RemotePeer::Impl
{
public:
    virtual ~Impl() =0;

    virtual std::string to_string() const noexcept =0;

    virtual void notify(const ChunkId& notice) =0;

    virtual ChunkId getNotice() =0;

    virtual void request(const ChunkId& request) =0;

    virtual ChunkId getRequest() =0;

    virtual void send(const MemChunk& chunk) =0;

    virtual WireChunk getChunk() =0;

    virtual void disconnect() =0;
};

RemotePeer::Impl::~Impl()
{}

/******************************************************************************/

/**
 * A remote peer that uses three `Wire`s to minimize latency.
 */
class RmtPeer3 final : public RemotePeer::Impl
{
private:
    SockAddr rmtAddr;    // Remote socket address
    Wire     noticeWire; // Wire for exchanging notices
    Wire     clntWire;   // Wire for sending requests and receiving chunks
    Wire     srvrWire;   // Wire for receiving requests and sending chunks

    SrvrSock createSrvrSock(
            InAddr    inAddr,
            PortPool& portPool)
    {
        const int n = portPool.size();
        int       i = 0;

        while (i++ < n) {
            const in_port_t port = portPool.take();
            SockAddr        srvrAddr(inAddr.getSockAddr(port));

            try {
                return SrvrSock(srvrAddr);
            }
            catch (const std::exception& ex) {
                portPool.add(port);
            }
        }

        throw RUNTIME_ERROR("Couldn't create server socket: all port number "
                "are in-use");
    }

public:
    /**
     * Server-side construction.
     *
     * @param[in] sock      `::accept()`ed socket
     * @param[in] portPool  Pool of potential port numbers
     */
    RmtPeer3(
            Socket&  noticeSock,
            PortPool portPool)
        : rmtAddr(noticeSock.getAddr())
        , noticeWire(noticeSock)
        , clntWire{}
        , srvrWire{}
    {
        noticeSock.setDelay(false);

        InAddr inAddr{rmtAddr.getInAddr()};

        // These listening server sockets will close when they go out of scope
        SrvrSock clntSrvrSock{createSrvrSock(inAddr, portPool)};
        SrvrSock srvrSrvrSock{createSrvrSock(inAddr, portPool)};

        clntSrvrSock.listen(1);
        srvrSrvrSock.listen(1);

        noticeWire.serialize(clntSrvrSock.getPort());
        noticeWire.serialize(srvrSrvrSock.getPort());
        noticeWire.flush();

        // TODO: Needs timeout
        // TODO: Accept only connections from `InAddr`
        Socket clntSock{clntSrvrSock.accept()};
        Socket srvrSock{srvrSrvrSock.accept()};

        clntSock.setDelay(false);
        srvrSock.setDelay(true); // Consolidate ACK and chunk

        clntWire = Wire(clntSock);
        srvrWire = Wire(srvrSock);
    }

    /**
     * Client-side construction.
     *
     * @param[in] rmtSrvrAddr            Socket address of the remote server
     * @throws    std::nested_exception  System error
     */
    RmtPeer3(const SockAddr& rmtSrvrAddr)
        : rmtAddr{rmtSrvrAddr}
        , noticeWire{}
        , clntWire{}
        , srvrWire{}
    {
        try {
            ClntSock noticeSock{rmtSrvrAddr};
            noticeSock.setDelay(false);

            noticeWire = Wire{noticeSock};

            in_port_t clntPort;
            in_port_t srvrPort;

            noticeWire.deserialize(srvrPort);
            noticeWire.deserialize(clntPort);

            ClntSock clntSock{rmtSrvrAddr.clone(clntPort)};
            ClntSock srvrSock{rmtSrvrAddr.clone(srvrPort)};

            clntSock.setDelay(false);
            srvrSock.setDelay(true); // Consolidate ACK and chunk

            clntWire = Wire{clntSock};
            srvrWire = Wire{srvrSock};
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("System error"));
        }
    }

    std::string to_string() const noexcept
    {
        return rmtAddr.to_string();
    }

    void notify(const ChunkId& notice)
    {
        notice.write(noticeWire);
        noticeWire.flush();
    }

    /**
     * Reads a notice
     *
     * @return                    Chunk ID
     * @throw  std::system_error  Couldn't read notice
     */
    ChunkId getNotice()
    {
        return ChunkId::read(noticeWire);
    }

    void request(const ChunkId& request)
    {
        request.write(clntWire);
        clntWire.flush();
    }

    ChunkId getRequest()
    {
        return ChunkId::read(srvrWire);
    }

    void send(const MemChunk& chunk)
    {
        chunk.write(srvrWire);
        srvrWire.flush();
    }

    WireChunk getChunk()
    {
        return WireChunk(clntWire);
    }

    /**
     * Disconnects from the remote peer. Idempotent.
     */
    void disconnect()
    {
        srvrWire = Wire();
        clntWire = Wire();
        noticeWire = Wire();
    }
};

/******************************************************************************/

RemotePeer::RemotePeer(Impl* const impl)
    : pImpl{impl}
{}

RemotePeer::RemotePeer(const SockAddr& srvrAddr)
    : RemotePeer{new RmtPeer3(srvrAddr)}
{}

RemotePeer::RemotePeer(
        Socket&   sock,
        PortPool& portPool)
    : RemotePeer{new RmtPeer3(sock, portPool)}
{}

std::string RemotePeer::to_string() const noexcept
{
    return pImpl->to_string();
}

void RemotePeer::notify(const ChunkId& notice)
{
    pImpl->notify(notice);
}

ChunkId RemotePeer::getNotice() {
    return pImpl->getNotice();
}

void RemotePeer::request(const ChunkId& request)
{
    pImpl->request(request);
}

ChunkId RemotePeer::getRequest()
{
    return pImpl->getRequest();
}

void RemotePeer::send(const MemChunk& chunk)
{
    pImpl->send(chunk);
}

WireChunk RemotePeer::getChunk()
{
    return pImpl->getChunk();
}

void RemotePeer::disconnect()
{
    pImpl->disconnect();
}

} // namespace
