/**
 * Factory for `Peer`s.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PeerFactory.cpp
 *  Created on: May 13, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "PeerFactory.h"
#include "Socket.h"

namespace hycast {

class PeerFactory::Impl final
{
private:
    PortPool     portPool;
    SrvrSock     srvrSock;
    PeerMsgRcvr& msgRcvr;

public:
    Impl(   const SockAddr& srvrAddr,
            const int       queueSize,
            PortPool&       portPool,
            PeerMsgRcvr&    msgRcvr)
        : portPool(portPool)
        , srvrSock(srvrAddr)
        , msgRcvr(msgRcvr) // Braces don't work
    {
        srvrSock.listen(queueSize);
    }

    in_port_t getPort()
    {
        return srvrSock.getPort();
    }

    /**
     * Creates an instance by accepting a connection from a remote peer. The
     * returned instance is not executing.
     *
     * @return  local instance
     */
    Peer accept()
    {
        Socket sock(srvrSock.accept());

        return Peer(sock, portPool, msgRcvr);
    }

    /**
     * Creates an instance by connecting to a remote server. The returned
     * instance is not executing.
     *
     * @param[in] rmtSrvrAddr            Socket address of the remote server
     * @return                           Local instance that's connected to a
     *                                   remote counterpart
     * @throws    std::nested_exception  System failure
     */
    Peer connect(const SockAddr& rmtSrvrAddr)
    {
        return Peer(rmtSrvrAddr, msgRcvr);
    }
};

/******************************************************************************/

PeerFactory::PeerFactory()
    : pImpl{}
{}

PeerFactory::PeerFactory(
        const SockAddr& srvrAddr,
        const int       queueSize,
        PortPool&       portPool,
        PeerMsgRcvr&    msgRcvr)
    : pImpl{new Impl(srvrAddr, queueSize, portPool, msgRcvr)}
{}

PeerFactory::PeerFactory(
        const InAddr& inAddr,
        const int     queueSize,
        PortPool&     portPool,
        PeerMsgRcvr&  msgRcvr)
    : PeerFactory{inAddr.getSockAddr(0), queueSize, portPool, msgRcvr}
{}

in_port_t PeerFactory::getPort() const
{
    return pImpl->getPort();
}

Peer PeerFactory::accept()
{
    return pImpl->accept();
}

Peer PeerFactory::connect(const SockAddr& rmtAddr)
{
    return pImpl->connect(rmtAddr);
}

} // namespace
